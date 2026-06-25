#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef _CDC_CONFIG_PARAM_PARSER_H
#define _CDC_CONFIG_PARAM_PARSER_H

#include "vencoder.h"
#include "expat.h"

// int parse_static_param(VencParamFromFiles *ve_param, XML_Char **attr, int i);

// int parse_common_param(VencParamFromFiles *ve_param, XML_Char **attr, int i);
int ParserCreateAndParse(XML_Parser parser, VencParamFromFiles *p_ve_param, FILE *xml_file);


#endif //_CDC_CONFIG_PARAM_PARSER_H

#ifdef __cplusplus
}
#endif /* __cplusplus */
