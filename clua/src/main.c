#include <string.h>

#include "mirua_module.h"
#include "mirua_module_internal.h"
#include "mirua_serialization.h"
int main(int argc, char const* argv[]) {
    (void)argc;
    (void)argv;
    MiruaNodeSerializer* serializer = mirua_serializer_get(SERIALIZER_TELEGRF);

    mirua_t_NodeList nodes;
    mirua_init_nodeList(&nodes, 128);
    serializer->deserialize_nodes("output2.txt", &nodes);

    char buf[1024 * 8];

    for (size_t i = 0; i < nodes.size; i++) {
        memset(buf, 0, sizeof buf);
        mirua_nodeId_to_string(buf, sizeof buf, &nodes.nodeIds[i], 0);
        printf("%s\n", buf);
    }

    return 0;
}
