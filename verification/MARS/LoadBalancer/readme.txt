this is our basic setup for LVS with direct routting:

                        ________
                       |        |
                       | client |
                       |________|
                   CIP=SGW=10.157.7.254 (eth0)
                           |
                           |
             __________    |
            |          |   |   (VIP=10.157.7.210, eth0:0)
            | director |---|
            |__________|   |   DIP=10.157.7.195 (eth0)
                           |
                           |
          -----------------------------------
          |                                 |
          |                                 |
 RIP=10.157.7.199(eth0)             RIP=10.157.7.200(eth0)
(VIP=10.157.7.210, lo:0)          (VIP=10.157.7.210, lo:0)
    ____________                       ____________
   |            |                     |            |
   | realserver |                     | realserver |
   |____________|                     |____________|



to access each of the VM boxes above use the following command:

        shh -X <vm_ip> 

the password each of the VMs is your user_name with suffix of 11. for example: erand11

after connecting to the VM in order to run the configuration scripts you will need root permissions. use the following commands:

        su - root

the password for root is 3tango.

To enable the above setup you will have to run 2 different scripts on the VM boxes:

1. in the director run the lvs_dr_lb.sh script (follow the usage instructions) 
2. in each of the real servers run the lvs_dr_rs.sh script. 

in case the scripts are not located on the VM, copy it using the following command:

        scp <path_to_script> root@<RIP>:~

password is 3tango. script will be copied to root. 

important note - in future setups there will probably different set of IPs. keep in mind to keep all of the VIP(virtual IP) and RIP (real IP) in the same subnet and with the same subnet mask. 





