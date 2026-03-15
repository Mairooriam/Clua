# Clua

Clua is OPC UA command line tool for browsing server and saving nodeids for telegraf opc ua .conf file made in C. Handles structures, arrays and primitive types. tested only with B&R opcua server.

Currently has basic browsing and saving of nodes in fromat for telegraf.

## Disclaimer

C Learning repository. AI has touched the code. Memory leaks present. bad code present.

## Browse and save
![Demo GIF](Resources/browse_and_save.gif)
## config
![Demo GIF](Resources/config_example.gif)
## structure
![Demo GIF](Resources/structure_example.png)
![Demo GIF](Resources/structure_example2.png)
### Prerequisites
Relies on open62541 for opc ua client. 

### TODO

- Get functionality up and running with replxx
- If connection fails promt user to use open65421 server ( runs a server on another thread and connects to it or single threeaded)
