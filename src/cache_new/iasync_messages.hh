#pragma once

#include "node.hh"

class async_messages {
  private:
    virtual ~async_messages() { }
    virtual void async_file_send  (std::string, Node&) = 0;
    virtual void async_data_send  (std::string, Node&) = 0;
    virtual void async_file_read  (std::string, Node&) = 0;
    virtual void async_file_write (std::string, Node&) = 0;

    virtual void async_request_file_read  (std::string, Node&) = 0;
    virtual void async_request_file_write (std::string, Node&) = 0;
    virtual void async_request_data_read  (std::string, Node&) = 0;
    virtual void async_request_data_write (std::string, Node&) = 0;
};
