#<b>ATC - accelerated TC</b>
--------------------------------
###<b>Overview</b>
****
~~~~~

The Linux® Traffic Control (TC) is a network
ingress and egress classification, action and traffic management subsystem. It
was first developed for the purpose of classifying packets and for traffic
management and virtual output queuing but has since evolved to support
arbitrary actions not related specifically to traffic management. 

The Mellanox Accelerated TC (ATC) solution improves upon the Linux TC by
moving the packet processing functionality from Linux network stack to the Indigo
NPS-400 network processor, installed in the Indigo NPS-400 Platform to improve
performance to up to 400Gb/s.

~~~~~~
###<b>Prepare ATC enviroment</b>
****

<b>1. Pull latest ATC from GitHub</b>
~~~
git clone git@github.com:Mellanox/ALVS.git
git checkout release/24.0100
~~~
<b>2. Download openNPU</b>

  [openNPU](http://opennpu.org/how-to-download/)
  ~~~
  
  Follow instructions in the above link on how to setup EZdk openNPU enviroment.
  
  The version must be 18_0400.
  ~~~

####<b>3. Remove EZdk soft link</b>
~~~
  Go to ALVS project folder and remove the soft link to EZdk. use the following commands:
  rm EZdk
~~~
####<b>4. Add soft link to openNPU EZdk</b>
~~~
Use the following command to link EZdk openNPU to ATC application:
ln -s <local openNPU folder>/EZdk EZdk
~~~
###<b>Build ATC application</b>
****

The ATC build process has 2 steps:

* Build ATC application

* Build ATC debian package

####<b>Compile ATC</b>
===
~~~
To compile the ATC application go to ALVS directory and run make atc command.
Make will compile the control plane (cp.mk) code and the data plane (dp.mk) code.
~~~
####<b>Build ATC debian package</b>
====
In order to build a debian package you must run the following command on a debian machine:

           make deb-atc
           
In case you don't have a debian machine, use a virtual machine running debian and mount

the ATC directory on it. Afterwards, perform the above command in the debian machine.

The output of the above command is a debian package file under ALVS folder. for example: 

          atc_24.0100.0000_amd64.deb
          
Copy the package to the NPS-400 appliance box and install it using the following command:

          dpkg -i atc_24.0100.0000_amd64.deb - also works for upgrade.
          
In case you want to remove ATC package from NPS-400 appliance, run the following command:

          dpkg -r atc
          
To purge (also removes ATC config):

          dpkg -P atc
          
For more information on ATC application, features, setup and more see:

* Manual [add link here]

* Release notes [add link here]
