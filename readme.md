# Clua

Clua is OPC UA command line tool for browsing server and saving nodeids for telegraf opc ua .conf file made in C. Handles structures, arrays and primitive types. tested only with B&R opcua server.
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

### Build Instructions
```bash
git clone https://github.com/yourusername/clua.git
cd clua
build.bat
```