#<b>ALVS - accelerated LVS</b>
--------------------------------

###<b>Overview</b>
****
~~~~~
The LVS is a SW load balancer implemented as part of the networking block of the Linux Kernel.
It hooks up to the netfilter framework in the Kernel which intercepts all incoming traffic
from network side and forwards it to the LVS.
The LVS responsibility is to spread the traffic between all attached real-servers according 
to a pre-defined scheduling algorithm configured by the network administrator. (Basic LVS configuration)
~~~~~~
###<b>Prepare ALVS enviroment</b>
****

<b>1. Pull latest ALVS from GitHub</b>
~~~
git clone git@github.com:Mellanox/ALVS.git
~~~
<b>2. Download openNPU</b>

  [openNPU](http://opennpu.org/how-to-download/)
  ~~~
  
  Follow instructions in the above link on how to setup EZdk openNPU enviroment. 
  
  The version must be 18_0300 or higher.
  ~~~
  
####<b>3. Remove EZdk soft link</b>
~~~
  Go to ALVS project folder and remove the soft link to EZdk. use the following commands:
  rm EZdk
~~~
####<b>4. Add soft link to openNPU EZdk</b>
~~~
  Use the following command to link EZdk openNPU to ALVS application:
  ln -s <local openNPU folder>/EZdk EZdk      
~~~
###<b>Build ALVS application</b>
****

The ALVS build process has 2 steps:

* Build ALVS application

* Build ALVS debian package

####<b>Compile ALVS</b>
===
~~~
To compile the ALVS application go to ALVS directory and run make command. 

Make will compile the control plane (cp.mk) code and the data plane (dp.mk) code. 
~~~

####<b>Build ALVS debian package</b>
====
In order to build a debian package you must run the following command on a debian machine:

            make –f deb.mk
  
In case you don't have a debian machine, use a virtual machine running debian and mount the ALVS

direcroty on it. Afterwards, perform the above command in the debian machine. 

The output of the above command is a debian package file under ALVS folder. for example:

            alvs_22.0200.0000_amd64.deb

Copy the package to the NPS-400 appliance box and install it using the following command:

            dpkg -i alvs_22.0200.0000_amd64.deb - also works for upgrade

In case you want to remove ALVS package from NPS-400 appliance, run the following command:

            dpkg -r alvs
    
To purge (also removes ALVS config):

            dpkg -P alvs

For more information on ALVS application, features, setup and more see:

* Manual [add link here]

* Release notes [add link here]
