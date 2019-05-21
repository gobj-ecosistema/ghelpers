/****************************************************************************
 *              JSON_XML.C
 *
 *  Conversión de xml a json,
 *  no procesa attributos (<tag attr="1">text</tag>), solo text.
 *
 *              Copyright (c) 2018 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include "21_json_xml.h"

/*****************************************************************
 *      Constants
 *****************************************************************/
#define MAX_TAG_NAME 1024
#define MAX_CONTENT_SIZE (8*1024)

typedef enum {
    ST_TRANSIT = 0,
    ST_INSIDE_START_TAG,
    ST_INSIDE_END_TAG,
    ST_INSIDE_CONTENT,
} subst_t;

#define CHANGE_STATE(new_state) \
    xml_parser->prev_subst = xml_parser->subst; \
    xml_parser->subst = (new_state);


/*****************************************************************
 *      Structures
 *****************************************************************/
typedef struct dl_node_s {
    DL_ITEM_FIELDS

    GBUFFER *tag;
    GBUFFER *endtag;
    GBUFFER *content;
    dl_list_t dl_childs;
    struct dl_node_s *parent;
} dl_node_t;

typedef struct {
    dl_list_t dl_root;
    dl_node_t *cur_node;
    subst_t prev_subst;
    subst_t subst;
    void (*startElement)(void *user_data, const char *name, const char **attrs);
    void (*endElement)(void *user_data, const char *name);
    void (*charData)(void *user_data, const char *s);
    void (*publishJson)(
        void *user_data,
        json_t *jn  // owned
    );
    void *user_data;
} xml_parser_t;

/*****************************************************************
 *      Data
 *****************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void free_node(dl_node_t *node)
{
    dl_node_t *child;

    while((child=dl_first(&node->dl_childs))) {
        dl_delete_item(child, 0);
        free_node(child);
    }
    GBUF_DECREF(node->tag);
    GBUF_DECREF(node->endtag);
    GBUF_DECREF(node->content);
    gbmem_free(node);
}

/***************************************************************************
 *  New node
 ***************************************************************************/
PRIVATE dl_node_t * new_node(xml_parser_t *xml_parser)
{
    dl_node_t *node = gbmem_malloc(sizeof(dl_node_t));
    if(!node) {
        log_error(0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "gbmem_malloc() FAILED",
            NULL
        );
        return 0;
    }
    node->tag = gbuf_create(256, MAX_TAG_NAME, 0, 0);
    node->endtag = gbuf_create(256, MAX_TAG_NAME, 0, 0);
    node->content = gbuf_create(256, MAX_CONTENT_SIZE, 0, 0);
    if(!xml_parser->cur_node) {
        // First node, the root
        dl_add(&xml_parser->dl_root, node);
    } else {
        dl_add(&xml_parser->cur_node->dl_childs, node);
        node->parent = xml_parser->cur_node;
    }
    xml_parser->cur_node = node;
    return node;
}

/***************************************************************************
 *  Return position of next char
 ***************************************************************************/
PRIVATE const char *nextchar(const char *p, int *len, char *pc, BOOL ign_blanks)
{
    int l = *len;
    const char *f = p;
    while(--l >= 0) {
        char c = *f;
        f++;
        if(ign_blanks && ((c== ' ' || c=='\t' || c=='\r' || c=='\n'))) {
            continue;
        }
        *len = l;
        *pc = c;
        return f;
    }
    *len = l;
    *pc = 0;
    return 0;
}

/***************************************************************************
 *  Check if its an array
 ***************************************************************************/
PRIVATE BOOL is_array(dl_list_t *dl)
{
    dl_node_t *node = dl_first(dl);
    char *endtag = gbuf_cur_rd_pointer(node->endtag);
    while(node) {
        char *endtag_ = gbuf_cur_rd_pointer(node->endtag);
        if(strcmp(endtag, endtag_)!=0) {
            return FALSE;
        }
        node = dl_next(node);
    }
    return TRUE;
}

/***************************************************************************
 *  dl 2 json
 *  XML Attributes are ignored! only content is processed.
 ***************************************************************************/
PRIVATE int dl2json(dl_list_t *dl, json_t *jn_obj)
{
    dl_node_t *node = dl_first(dl);
    while(node) {
        char *endtag = gbuf_cur_rd_pointer(node->endtag);
        char *content = gbuf_cur_rd_pointer(node->content);
        int childs = dl_size(&node->dl_childs);
        if(childs) {
            /*
             *  Childs prevail over content
             */
            if(!is_array(&node->dl_childs)) {
                json_t *child_jn_obj = json_object();
                json_object_set_new(jn_obj, endtag, child_jn_obj);
                dl2json(&node->dl_childs, child_jn_obj);
            } else {
                json_t *child_jn_obj = json_object();
                json_object_set_new(jn_obj, endtag, child_jn_obj);


                json_t *jn_list = json_array();
                dl_node_t *node_list = dl_first(&node->dl_childs);
                char *endtag_ = gbuf_cur_rd_pointer(node_list->endtag);
                json_object_set_new(child_jn_obj, endtag_, jn_list);
                while(node_list) {

                    json_t *child_jn_obj = json_object();
                    json_array_append_new(jn_list, child_jn_obj);
                    dl2json(&node_list->dl_childs, child_jn_obj);

                    node_list = dl_next(node_list);
                }
            }
        } else {
            json_object_set_new(jn_obj, endtag, json_string(content?content:""));
        }
        node = dl_next(node);
    }
    return 0;
}

/***************************************************************************
 *  Convert xml to dl_list
 ***************************************************************************/
PRIVATE int xml2dl(xml_parser_t *xml_parser, const char *p, int ln)
{
    int *len = &ln;
    char c;
    BOOL fin = FALSE;
    while(!fin) {
        switch(xml_parser->subst) {
            case ST_TRANSIT:
                p = nextchar(p, len, &c, TRUE);
                if(!p) {
                    return 0;
                }
                /*
                 *  First char must be <
                 */
                if(c == '<') {
                    CHANGE_STATE(ST_INSIDE_START_TAG)
                } else {
                    trace_msg("BASURA %s", p);
                    return -1;
                }
                break;

            case ST_INSIDE_START_TAG:
                p = nextchar(p, len, &c, FALSE);
                if(!p) {
                    return 0;
                }
                if(xml_parser->prev_subst != ST_INSIDE_START_TAG) {
                    if(c == '/') {
                        CHANGE_STATE(ST_INSIDE_END_TAG)
                        break;
                    }
                    CHANGE_STATE(ST_INSIDE_START_TAG)
                    dl_node_t *node = new_node(xml_parser);
                    if(!node) {
                        return -1;
                    }
                }
                if(c == '>') {
                    if(xml_parser->startElement) {
                        xml_parser->startElement(
                            xml_parser->user_data,
                            gbuf_cur_rd_pointer(xml_parser->cur_node->tag),
                            0
                        );
                    }
                    CHANGE_STATE(ST_INSIDE_CONTENT)
                } else {
                    gbuf_append(xml_parser->cur_node->tag, &c, 1);
                }
                break;

            case ST_INSIDE_CONTENT:
                p = nextchar(p, len, &c, FALSE);
                if(!p) {
                    return 0;
                }
                if(c == '<') {
                    if(xml_parser->charData) {
                        xml_parser->charData(
                            xml_parser->user_data,
                            gbuf_cur_rd_pointer(xml_parser->cur_node->content)
                        );
                    }
                    CHANGE_STATE(ST_INSIDE_START_TAG)
                } else {
                    gbuf_append(xml_parser->cur_node->content, &c, 1);
                }
                break;

            case ST_INSIDE_END_TAG:
                p = nextchar(p, len, &c, FALSE);
                if(!p) {
                    return 0;
                }
                if(c == '>') {
                    /*
                     *  Check if tags match
                     */
                    /*
                     *  TODO en t1 pueden venir atributos, hay que quitarlos
                     *  ponerlos aparte.
                     */
                    char *t1 = gbuf_cur_rd_pointer(xml_parser->cur_node->tag);
                    char *t2 = gbuf_cur_rd_pointer(xml_parser->cur_node->endtag);
                    //int ln1 = gbuf_leftbytes(xml_parser->cur_node->tag);
                    int ln2 = gbuf_leftbytes(xml_parser->cur_node->endtag);
                    if(memcmp(t1, t2, ln2)!=0) {
                        log_error(0,
                            "gobj",         "%s", __FILE__,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_XML_ERROR,
                            "msg",          "%s", "unmatch start/end tags",
                            "start-tag",    "%s", t1,
                            "end-tag",      "%s", t2,
                            NULL
                        );
                        return -1;
                    }
                    CHANGE_STATE(ST_TRANSIT)

                    if(xml_parser->endElement) {
                        xml_parser->endElement(
                            xml_parser->user_data,
                            gbuf_cur_rd_pointer(xml_parser->cur_node->endtag)
                        );
                    }

                    xml_parser->cur_node = (xml_parser->cur_node)->parent;
                    if(!xml_parser->cur_node) {
                        if(xml_parser->publishJson) {
                            json_t *jn_obj = json_object();
                            dl2json(&xml_parser->dl_root, jn_obj);
                            xml_parser->publishJson(
                                xml_parser->user_data,
                                jn_obj
                            );
                        }
                        xml_reset(xml_parser);
                    }

                } else {
                    gbuf_append(xml_parser->cur_node->endtag, &c, 1);
                }
                break;
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void *xml_init(
    void *user_data,
    void (*startElement)(
        void *user_data,
        const char *name,
        const char **attrs  // Not implemented
    ),
    void (*endElement)(void *user_data, const char *name),
    void (*charData)(void *user_data, const char *s),
    void (*publishJson)(
        void *user_data,
        json_t *jn  // must be owned
    )
)
{
    xml_parser_t *xml_parser = gbmem_malloc(sizeof(xml_parser_t));
    dl_init(&xml_parser->dl_root);

    xml_parser->user_data = user_data;
    xml_parser->startElement = startElement;
    xml_parser->endElement = endElement;
    xml_parser->charData = charData;
    xml_parser->publishJson = publishJson;

    return xml_parser;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int xml_end(void *parser)
{
    xml_reset(parser);
    gbmem_free(parser);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int xml_reset(void *parser)
{
    xml_parser_t *xml_parser = parser;
    dl_node_t *node;

    while((node=dl_first(&xml_parser->dl_root))) {
        dl_delete_item(node, 0);
        free_node(node);
    }

    xml_parser->prev_subst = ST_TRANSIT;
    xml_parser->subst = ST_TRANSIT;
    xml_parser->cur_node = 0;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int xml_parse(
    void *parser,
    char *bf,
    int len
)
{
    int ret = xml2dl(parser, bf, len);
    if(ret < 0) {
        xml_reset(parser);
    }
    return ret;
}

