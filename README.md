# S-talk - Inter process communication system

<img src="https://user-images.githubusercontent.com/84153519/213350391-bcfb9cc8-d50c-4200-828a-bc8d27f9d99b.png" width="300" height="300">

S-talk is a system for inter-process communication (IPC) that allows two users to communicate with each other in real-time. The system uses a client-server model, where one user acts as the server and the other acts as the client. 

To initiate an s-talk session, two users must first agree on the following:
- The machine that each will be running on
- The port number that each will use

For example, let's say Fred and Barney want to initiate an s-talk session. Fred is on machine "csil-cpu1" and will use port number 6060. Barney is on machine "csil-cpu3" and will use port number 6001. 

To initiate the s-talk session, Fred must type the following command:
> s-talk 6060 csil-cpu3 6001

This tells the s-talk system to use port number 6060 on his machine, and connect to port number 6001 on machine "csil-cpu3". 

Similarly, Barney must type the following command:
> s-talk 6001 csil-cpu1 6060

This tells the s-talk system to use port number 6001 on his machine, and connect to port number 6060 on machine "csil-cpu1".

Once the session is initiated, every line typed at each terminal will appear on BOTH terminals, appearing character-by-character on the sender’s terminal as they type it, and it will appear on the receiver’s terminal once the sender presses enter.

An s-talk session can be terminated by either user by entering the complete line of text '!' and pressing enter.

It's important to note that the IP address here is taken in from the devices/machine names of different linux cpus in SFU computer labs. For a normal linux PC, the IP address needs to be used instead of the machine name.
