#!/usr/bin/env ruby

require 'socket'

class MapReduce
  attr_reader @role, @master_socket, @cache_socket, @fd_read, @fd_write
  attr_accessor @textFile
  @input_pathes = [ ]

  public
  def initialize
    if ARGV.length > 1 then
      input  = IO.new args[1]
      output = IO.new args[2]
      role = args[3]
    else 
      role = 'JOB'
    end

    load_config_file
    case role
    when 'MAP', 'REDUCE'
      connect_to_cache
      connect_to_slave
    when 'JOB'
      connect_to_master
    end
  end

  def map

  end

  def reduce 

  end

  private
  def load_config_file
  end

  def connect_to_cache
    cache_socket = TCPSocket.new(cache_addr, cache_port)
  end

  def connect_to_slave
    input << "requestconf"
    output >> message
    @jobid, @taskid = message.sub(/^taskconf /, '').split(' ')

    while output >> message 
      if message ~= /inputpath/ 
        message.sub!(/^inputpath /, '')
        @input_pathes = message.split(' ')

      elsif message ~= /Einput/
        break
      end
    end

  end

  def connect_to_master
    master_socket = TCPSocket.new(master_addr, master_port)

    line = master_socket.gets
    if line == "whoareyou" then
      master_socket.puts "job"

      while (line = master_socket.gets).length <= 0 
      end

      if line == 'jobid' then
        id = line.sub(/^jobid/. ' ')
      end
     end
  end

  def close 
    cache_socket.close
  end
end

