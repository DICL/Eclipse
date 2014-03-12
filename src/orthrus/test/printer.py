class porthrus:
  def __init__(self, val):
    self.val = val
  
  def policy (self):
    opt = "CACHE_LRU","CACHE_SPATIAL","CACHE_SYNCHRONIZE"
    return  str(opt[int(self.val['policies'])])

  def setted (self):
    val = int(self.val['setted'])
    output = "missing setting"
    if val == 0b111111:
      output = "SETTED_ALL"
    return output
 
  def status (self):
    opt = "STATUS_VIRGIN ", "STATUS_LOADED ", "STATUS_READY  ", "STATUS_RUNNING", "STATUS_CLOSED " 
    return  str(opt[int(self.val['status'])])

  def to_string (self):
    output = "\n"
    output += "------------------------------------" + '\n'
    output += "+++ ORTHRUS DEBUGGER ---------------" + '\n'
    output += "------------------------------------" + '\n'
    output += " - POLICY -> " + self.policy() + '\n'
    output += " - SETTED -> " + self.setted() + '\n'
    output += " - STATUS -> " + self.status() + '\n'
    output += " - IP     -> " + str(self.val['local_ip_str']).strip('" ') + '\n'
    output += " - HOST   -> " + str(self.val['host']).strip('" ') + '\n'
    output += " - INDEX  -> " + str(self.val['local_no']).strip('" ') + '\n'
    output += "------------------------------------" + '\n'
    return output

def lookup_type (val):
  if str(val.type) == 'Orthrus':
    return porthrus(val)
  else:
    return None

gdb.pretty_printers.append (lookup_type)
