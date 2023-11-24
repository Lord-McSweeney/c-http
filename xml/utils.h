#include "attributes.h"
#include "nodes.h"

#ifndef _XML_UTILS
    #define _XML_UTILS 1

struct xml_node *XML_getElementById(struct xml_node node, const char *id) {
    struct xml_list list;
    list.count = 0;
    list.nodes = NULL;
    
    XML_descendants(&list, node);
    for (int i = 0; i < list.count; i ++) {
        struct xml_node current_node = list.nodes[i];
        struct xml_attrib_result attribs = XML_parseAttributes(node.attribute_content);

        if (!attribs.error) {
            struct xml_attributes *attributes = attribs.attribs;
            char *idAttribute = XML_getAttributeByName(attributes, "id");
            if (idAttribute && !strcmp(idAttribute, id)) {
                return (list.nodes + i);
            }

            freeXMLAttributes(attributes);
        }
    }

    return NULL;
}

#endif
