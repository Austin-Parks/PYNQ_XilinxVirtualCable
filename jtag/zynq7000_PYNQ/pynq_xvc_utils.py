import os
import time
import pynq

# The following function will search through the PYNQ OVerlay ip_dict and
# returns the matching ip_dict['xxx'] metadata entry for the given PYNQ 
# Overlay Device IP (type == pynq.DefaultIP). Returns None if no match was
# found.
def get_dev_ip_dict(pynq_dev:pynq.DefaultIP, dbg=0):
    find_addr = pynq_dev.mmio.base_addr
    dbg and print(f"find_addr: 0x{find_addr:08X}")
    find_key = None
    for key, val in pynq_dev.device.ip_dict.items():
        if('phys_addr' in val.keys()):
            if(val['phys_addr'] == find_addr):
                find_key = key
                break
    result = None
    if(find_key):
        dbg and print(f"find_key: {find_key:s}")
        result = pynq_dev.device.ip_dict[key]
    return result
        

# This will use linux bash commands to kill any running instances of XVC server
def stop_servers():
    os.system('killall -qr "xvcServer*"')

# This will search through a PYNQ Overlay object and launch any XVC server instances for
# any Debug Bridge IP Found in the design. Ports start at 2542 and increment by 100 For
# each additional debug bridge instance...
def start_servers(ol:pynq.Overlay, dbg=0):
    os.system('killall -qr "xvcServer*"')
    # Look for debug bridge IPs in the design and print their BASE ADDRESS(s)
    i = 0
    xvc_list = []
    os.system(f"echo '' > .xvc_logs")
    for key, val in ol.ip_dict.items():
        if('debug_bridge' in str(val['type']) ):
            # We only instantiate XVC Servers for Bebug Bridge instances using the
            # following Debug Modes:
            # C_DEBUG_MODE = 2 (AXI-to-BSCAN)
            #             or
            # C_DEBUG_MODE = 3 (AXI-to-JTAG)
            if(val['parameters']['C_DEBUG_MODE'] == '2' or val['parameters']['C_DEBUG_MODE'] == '3' ):
                addr_str = f"0x{val['phys_addr']:08X}"
                port = ( 2542 + (i * 100) )
                print(f"Found debug bridge IP: {val['fullpath']:>32s}  ->  BASE ADDRESS: {addr_str}  |  XVC PORT: {port:d}")
                os.system(f"./xvc.sh {addr_str} {port:d} >> .xvc_logs")
                xvc_inst = {}
                xvc_inst['name'] = val['fullpath']
                xvc_inst['addr'] = val['phys_addr']
                xvc_inst['hexs'] = addr_str
                xvc_inst['port'] = port
                xvc_list.append(xvc_inst)
                i += 1
                time.sleep(2.0)
    print(f"")
    print(f"Started XVC Server processe(s):\n")
    os.system(f"ps aux | grep -v 'grep' | grep 'sudo' | grep 'xvcServer' -m {(3*len(xvc_list)):d}  | awk -F ' ' '"+"{print $2}' > .xvc_pids")
    os.system(f"ps aux | grep -v 'grep' | grep 'sudo' | grep 'xvcServer' -m {(3*len(xvc_list)):d}  | awk -F 'sudo' '"+"{print $2}' > .xvc_cmds")
    with open('.xvc_cmds', "r") as f_cmds:
        cmds_out = f_cmds.readlines()
        for cmd in cmds_out:
            print(f"    ~/$ {cmd}")
    return xvc_list