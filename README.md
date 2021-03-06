DistributedLevelDB
==================

A distrubted database utilizing google leveldb for backend storage.

INSTALLATION:
=================
I. install dependencies:
   1. leveldb: https://code.google.com/p/leveldb/
   2. jsoncpp: http://jsoncpp.sourceforge.net/

II. run `make`

DESIGN DOCUMENTATION:
=================
I. client:

client parse requests into json messages, then send to gateway server via client::sendstring() api. by default, gateway server enforces strong consistency to client requests, meaning gateway server replies to client only when data is fully replicated to all leveldb servers. however, client has the option to tell gateway server to be eventual consistent, meaning a put operation will reply as soon as 1 db server succeeds.

II. server:

1. gateway server: gateway server accepts requests from client, then contact a subset of all leveldb servers to process the client request. the subset is selected via a cluster manager, which is implemented as a separate thread of the gateway server process and runs as a separate server (i.e. the cluster manager has its own port). whenever a leveldb server joins, it sends a LEVELDBSERVER_JOIN json request to any cluster server. the cluster server then assigns leveldb server to the smallest cluster. each cluster server has indexing data to keep track of the cluster sizes and leveldb to cluster id information. those indexing data are kept consistent across all cluster servers--whenever any cluster server changes its local indexing data, it broadcasts its new indexing data to all other cluster servers for update. client requests are received by gateway server, then passed down to that gatewayserver's cluster server. cluster server hashes client request key to find the corresponding cluster id for the request, and cluster server's indexing data has mapping information for the set of leveldb servers for that particular cluster id.

2. leveldb server: leveldb server interfaces with leveldb api's. it accepts requests as json messages, then parse out the key and value parts. it returns leveldb api's return falues as json strings to gateway server. each leveldb server belongs to exactly one cluster. its cluster id is determined by hash(levedb server ip, leveldb server port).

EXAMPLES:
=================
1) start gateway server:
$ ./g.out

2) start leveldb servers (run the same common for EACH leveldb server):
$ ./l.out -clusterip <gateserver cluster ip> -clusterport <gateserver cluster port> -selfport <leveldb server port> -dbdir <db directory>

3) test client:
$ ./c.out