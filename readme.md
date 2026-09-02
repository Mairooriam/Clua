# Clua

Clua is OPC UA command line tool for browsing server and saving nodeids for telegraf opc ua .conf file made in C. Handles structures, arrays and primitive types. tested only with B&R opcua server.

Currently has basic browsing and saving of nodes in fromat for telegraf.

## Disclaimer

C Learning repository. AI has touched the code. Memory leaks present. bad code present.

## Build

cmake ..
cmake -S .. -DCMAKE_BUILD_TYPE=Debug

cmake --build .

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

- [] go trough cmake to get rid of global compile commands. they pollute subdirectories.
 

