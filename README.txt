
1. Author/email address

	Nikhta Chamarti (nc20ce)
	Sai Sowjanya Padamati (sp22bi)

2. How to compile the code

    platform: linux
    "make all" would compile both the bridge and station
	"make clean" would remove the .cs* file and bridge and station executables


3. Commands supported in stations/routers/bridges
	
	Only stations:
	   send <destination> <message> // send message to a destination host

	Station and Router:
	   show	arp 		// show the ARP cache table information
	   show	pq 		 	// show the pending_queue
	   show	host 		// show the IP/name mapping table
	   show	iface 		// show the interface information
	   show	rtable 		// show the contents of routing table
	   quit // close the router

   	Bridges:

	   show sl 		// show the contents of self-learning table
	   quit // close the bridge


4. To start the emulation, run

   	Run the program:
	
	Connect to a bridge: 
		cs1: "./bridge cs1 8"  or  "run b1"
		cs2: "./bridge cs2 8"  or  "run b2"
		cs3: "./bridge cs3 8"  or  "run b3"

	Connect to a station:
		A:  "./station -no ifaces/ifaces.a rtables/rtable.a hosts"  or  "run A"
		B:  "./station -no ifaces/ifaces.b rtables/rtable.b hosts"  or  "run B"
		C:  "./station -no ifaces/ifaces.c rtables/rtable.c hosts"  or  "run C"
		D:  "./station -no ifaces/ifaces.d rtables/rtable.d hosts"  or  "run D"
		E:  "./station -no ifaces/ifaces.e rtables/rtable.e hosts"  or  "run E"

	Connect to a router:
		R1: "./station -route ifaces/ifaces.r1 rtables/rtable.r1 hosts"  or  "run R1"
		R2: "./station -route ifaces/ifaces.r2 rtables/rtable.r2 hosts"  or  "run R2"


    Emulates the following network topology

   
          B              C                D
          |              |                |
         cs1-----R1------cs2------R2-----cs3
          |              |                |
          -------A--------                E

    cs1, cs2, and cs3 are bridges.
    R1 and R2 are routers.
    A to E are hosts/stations.
    Note that A is multi-homed, but it is not a router.