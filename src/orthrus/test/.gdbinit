python execfile('printer.py')

define load_mrr
 printf "MRR LOADED\n"
end

define pcache
 printf "------------------------------------------------------\n"
 echo    ++++++++    \033[31mCACHE STATE\n\033[0m
 printf "------------------------------------------------------\n"
 set $st = this->status 
 printf " - STATUS:   "
 if $st == 0
  printf "STATUS_VIRGIN\n"
 else 
  if $st == 1
   printf "STATUS_LOADED\n"
  else 
   if $st == 2
    printf "STATUS_READY \n"
   else
    if $st == 3
     printf "STATUS_RUNNING\n"
    else
     if $st == 4
      printf "STATUS_CLOSED\n"
     end  
    end  
   end  
  end
 end

 printf " - SETTED:   "
 if this->setted = 111111
   printf "ALL_SETTED\n"
 else
  print/t this->setted 
 end 
 set $po = this->policies
 printf " - POLICY:   "
 if $po == 0
  printf "CACHE_LRU\n"
 else 
  if $po == 1
   printf "CACHE_SPATIAL\n"
  else 
   if $po == 2
    printf "CACHE_SYNCHRONIZE\n"
   else
    if $po == 4
     printf "CACHE_PUSH\n"
    else
     if $po == 16
      printf "CACHE_PULL\n"
     end  
    end  
   end  
  end
 end
 printf " - IP:       %s\n", this->local_ip_str
 printf " - HOST:     %s\n", this->host
 printf " - INDEX:    %d\n", this->local_no
 printf "------------------------------------------------------\n"
end
