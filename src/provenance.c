//
//  provenance.c
//  libprov
//
//  This library is being developed based on the draft model of the
//  W3C provenance working group.
//  http://dvcs.w3.org/hg/prov/raw-file/0f43d8bc798c/model/ProvenanceModel.html
//  https://github.com/lucmoreau/ProvToolbox/blob/master/xml/src/main/resources
//
//  Created by Satrajit Ghosh on 11/25/11.
//  Copyright (c) 2011 TankThink Labs LLC. All rights reserved.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#if defined(LIBXML_XPATH_ENABLED) && defined(LIBXML_SAX1_ENABLED)

#include "provenance.h"

typedef struct {
    xmlDocPtr doc;
} PrivateProv;

typedef PrivateProv *PrivateProvPtr;

typedef struct{
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
} XPathQuery;

typedef XPathQuery *XPathQueryPtr;

/*Initialization and Cleanup routines*/
/*
Create a provenance object that can be used by a client to generate an
instance of a provenance schema
*/
ProvPtr create_provenance_object(const char* id)
{
    //fprintf(stdout, "Creating provenance object [BEGIN]\n");
    LIBXML_TEST_VERSION;
    ProvPtr p_prov = (ProvPtr)malloc(sizeof(Provenance));
    if(p_prov == NULL) {
        fprintf(stderr, "Error: unable to create Provenance struct\n");
        return(NULL);
    }
    assert(p_prov);
    assert(id);
    p_prov->id = strdup(id);
    p_prov->private = (void *)malloc(sizeof(PrivateProv));

    PrivateProvPtr p_priv = (PrivateProvPtr)(p_prov->private);
    p_priv->doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "record");
    xmlSetProp(root_node, BAD_CAST "id", BAD_CAST id);
    xmlDocSetRootElement(p_priv->doc, root_node);
    //fprintf(stdout, "Creating provenance object [END]\n");
    return(p_prov);
}

/*
Clean up the memory used by the xml document
*/
int destroy_provenance_object(ProvPtr p_prov)
{
    assert(p_prov);
    //fprintf(stdout, "Destroying provenance object [BEGIN]\n");
    PrivateProvPtr p_priv = (PrivateProvPtr)(p_prov->private);

    /*free the document */
    xmlFreeDoc(p_priv->doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();

    /*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();
    free(p_priv);
    free(p_prov->id);
    free(p_prov);
    //fprintf(stdout, "Destroying provenance object [END]\n");

    return(0);
}

/*
print provenance info to stdout (filename==NULL) or to a file
*/
void print_provenance(ProvPtr p_prov, const char *filename)
{
    assert(p_prov);
    //fprintf(stdout, "Printing provenance\n");
    //fprintf(stdout, "ID: %d\n", p_prov->id);

    /*Get the root element node */
    PrivateProvPtr p_priv = (PrivateProvPtr)(p_prov->private);
    if (filename == NULL)
        xmlSaveFormatFileEnc("-", p_priv->doc, "UTF-8", 1);
    else
        xmlSaveFormatFileEnc(BAD_CAST filename, p_priv->doc, "UTF-8", 1);
}

/* Private U=utility routines */

/*
Query the document for elements using xpath
*/
static XPathQueryPtr query_xpath(const xmlDocPtr doc, const xmlChar* xpathExpr)
{
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;

    /* Create xpath evaluation context */
    xpathCtx = xmlXPathNewContext(doc);
    if(xpathCtx == NULL) {
        fprintf(stderr,"Error: unable to create new XPath context\n");
        return(NULL);
    }

    /* Evaluate xpath expression */
    xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    if(xpathObj == NULL) {
        fprintf(stderr,"Error: unable to evaluate xpath expression \"%s\"\n", xpathExpr);
        xmlXPathFreeContext(xpathCtx);
        return(NULL);
    }
    XPathQueryPtr p_xpquery = (XPathQueryPtr)malloc(sizeof(XPathQuery));
    p_xpquery->xpathCtx = xpathCtx;
    p_xpquery->xpathObj = xpathObj;
    return p_xpquery;
}

static int free_xpquery(XPathQueryPtr p_xpquery){
    xmlXPathFreeObject(p_xpquery->xpathObj);
    xmlXPathFreeContext(p_xpquery->xpathCtx);
    free(p_xpquery);
    return(0);
}


/*
Generic element adding routine
*/
static xmlNodePtr add_element(ProvPtr p_prov,
                              const char* root,
                              const char* element_name,
                              const char* id_prefix)
{
    PrivateProvPtr p_priv = (PrivateProvPtr)(p_prov->private);
    xmlNodePtr p_node, root_node;
    xmlChar id[255];
    int size;
    xmlChar xpathExpr[255];

    sprintf(xpathExpr, "//%s/%s", root, element_name);
    XPathQueryPtr p_xpquery = query_xpath(p_priv->doc,
                                          xpathExpr);

    size = (p_xpquery->xpathObj->nodesetval) ? p_xpquery->xpathObj->nodesetval->nodeNr : 0;
    free_xpquery(p_xpquery);

    assert(id_prefix);
    sprintf(id, "%s%d", id_prefix, size);

    sprintf(xpathExpr, "//%s", root);
    p_xpquery = query_xpath(p_priv->doc, xpathExpr);

    size = (p_xpquery->xpathObj->nodesetval) ? p_xpquery->xpathObj->nodesetval->nodeNr : 0;
    if (size == 0)
        root_node = xmlNewChild(xmlDocGetRootElement(p_priv->doc), NULL, BAD_CAST root, NULL);
    else
        root_node = p_xpquery->xpathObj->nodesetval->nodeTab[0];
    free_xpquery(p_xpquery);

    //fprintf(stdout, "%s:", id);
    p_node = xmlNewChild(root_node, NULL, BAD_CAST element_name, NULL);
    xmlNewProp(p_node, BAD_CAST "id", BAD_CAST id);

    return p_node;
}

/* Record creation routines */

IDREF add_entity(ProvPtr p_prov)
{
    xmlNodePtr p_node = add_element(p_prov, "record", "entity", "e");
    return(xmlGetProp(p_node, "id"));
}

IDREF add_activity(ProvPtr p_prov, const char* recipeLink,
                   const char* startTime, const char* endTime)
{
    xmlNodePtr p_node = add_element(p_prov, "record", "activity", "a");
    if (startTime != NULL)
        xmlNewChild(p_node, NULL, BAD_CAST "startTime", BAD_CAST startTime);
    if (endTime != NULL)
        xmlNewChild(p_node, NULL, BAD_CAST "endTime", BAD_CAST endTime);
    return(xmlGetProp(p_node, "id"));
}

IDREF add_agent(ProvPtr p_prov)
{
    xmlNodePtr p_node = add_element(p_prov, "record", "agent", "ag");
    return(xmlGetProp(p_node, "id"));
}

IDREF add_note(ProvPtr p_prov)
{
    xmlNodePtr p_node = add_element(p_prov, "record", "note", "n");
    return(xmlGetProp(p_node, "id"));
}

/* add relations */
IDREF add_usedRecord(ProvPtr p_prov, IDREF activity, IDREF entity, const char* time)
{
    xmlNodePtr p_node = add_element(p_prov, "dependencies", "used", "u");
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "effect", NULL), BAD_CAST "ref", BAD_CAST activity);
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "cause", NULL), BAD_CAST "ref", BAD_CAST entity);
    if (time != NULL)
        xmlNewChild(p_node, NULL, BAD_CAST "time", BAD_CAST time);
    return(xmlGetProp(p_node, "id"));
}

IDREF add_generatedByRecord(ProvPtr p_prov, IDREF entity, IDREF activity, const char* time)
{
    xmlNodePtr p_node = add_element(p_prov, "dependencies", "wasGeneratedBy", "wgb");
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "effect", NULL), BAD_CAST "ref", BAD_CAST entity);
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "cause", NULL), BAD_CAST "ref", BAD_CAST activity);
    if (time != NULL)
        xmlNewChild(p_node, NULL, BAD_CAST "time", BAD_CAST time);
    return(xmlGetProp(p_node, "id"));
}

IDREF add_controlledByRecord(ProvPtr p_prov, IDREF activity, IDREF agent, const char* startTime, const char* endTime)
{
    xmlNodePtr p_node = add_element(p_prov, "dependencies", "wasControlledBy", "wcb");
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "effect", NULL), BAD_CAST "ref", BAD_CAST activity);
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "cause", NULL), BAD_CAST "ref", BAD_CAST agent);
    if (startTime != NULL)
        xmlNewChild(p_node, NULL, BAD_CAST "startTime", BAD_CAST startTime);
    if (endTime != NULL)
        xmlNewChild(p_node, NULL, BAD_CAST "endTime", BAD_CAST endTime);
    return(xmlGetProp(p_node, "id"));
}

IDREF add_DerivedFromRecord(ProvPtr p_prov, IDREF entity_effect, IDREF entity_cause)
{
    xmlNodePtr p_node = add_element(p_prov, "dependencies", "wasDerivedFrom", "wdf");
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "effect", NULL), BAD_CAST "ref", BAD_CAST entity_effect);
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "cause", NULL), BAD_CAST "ref", BAD_CAST entity_cause);
    return(xmlGetProp(p_node, "id"));
}

IDREF add_informedByRecord(ProvPtr p_prov, IDREF activity_effect, IDREF activity_cause, const char* time)
{
    xmlNodePtr p_node = add_element(p_prov, "dependencies", "wasInformedBy", "wib");
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "effect", NULL), BAD_CAST "ref", BAD_CAST activity_effect);
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "cause", NULL), BAD_CAST "ref", BAD_CAST activity_cause);
    if (time != NULL)
        xmlNewChild(p_node, NULL, BAD_CAST "time", BAD_CAST time);
    return(xmlGetProp(p_node, "id"));
}

IDREF add_hasAnnotationRecord(ProvPtr p_prov, IDREF thing, IDREF note)
{
    xmlNodePtr p_node = add_element(p_prov, "dependencies", "hasAnnotation", "ha");
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "thing", NULL), BAD_CAST "ref", BAD_CAST thing);
    xmlSetProp(xmlNewChild(p_node, NULL, BAD_CAST "note", NULL), BAD_CAST "ref", BAD_CAST note);
    return(xmlGetProp(p_node, "id"));
}

/* Record manipulation routines */

/*
Add key value attributes to an element with given id
*/
int add_attribute(ProvPtr p_prov, const char* id, const char* key, const char* value)
{
    PrivateProvPtr p_priv = (PrivateProvPtr)(p_prov->private);
    xmlChar xpathExpr[128];
    sprintf(xpathExpr, "//*[@id='%s']", id);
    XPathQueryPtr p_xpquery = query_xpath(p_priv->doc,
                                          xpathExpr);
    int size = (p_xpquery->xpathObj->nodesetval) ? p_xpquery->xpathObj->nodesetval->nodeNr : 0;
    //fprintf(stdout, "xpath[%s]:numnodes[%d]\n", xpathExpr, size);
    if (size==0){
       fprintf(stderr, "Element with id=%s not found.\n", id);
       /* Cleanup */
       free_xpquery(p_xpquery);
       return(1);
    }
    if (size>1){
       fprintf(stderr, "Multiple elements found. Doc invalid.\n");
       /* Cleanup */
       free_xpquery(p_xpquery);
       return(1);
    }
    xmlNodePtr p_node = p_xpquery->xpathObj->nodesetval->nodeTab[0];
    assert(key);
    assert(value);
    xmlNewChild(p_node, NULL, BAD_CAST key, BAD_CAST value);

    /* Cleanup */
    free_xpquery(p_xpquery);
    return(0);
}

/*
Sets properties of an element
*/
int set_property(ProvPtr p_prov, IDREF id, const char* name, const char* value)
{
    PrivateProvPtr p_priv = (PrivateProvPtr)(p_prov->private);
    xmlChar xpathExpr[128];
    sprintf(xpathExpr, "//*[@id='%s']", id);
    XPathQueryPtr p_xpquery = query_xpath(p_priv->doc,
                                          xpathExpr);
    int size = (p_xpquery->xpathObj->nodesetval) ? p_xpquery->xpathObj->nodesetval->nodeNr : 0;
    //fprintf(stdout, "xpath[%s]:numnodes[%d]\n", xpathExpr, size);
    if (size==0){
       fprintf(stderr, "Element with id=%s not found.\n", id);
       /* Cleanup */
       free_xpquery(p_xpquery);
       return(1);
    }
    if (size>1){
       fprintf(stderr, "Multiple elements found. Doc invalid.\n");
       /* Cleanup */
       free_xpquery(p_xpquery);
       return(1);
    }
    xmlNodePtr p_node = p_xpquery->xpathObj->nodesetval->nodeTab[0];
    assert(name);
    assert(value);
    xmlSetProp(p_node, BAD_CAST name, BAD_CAST value);

    /* Cleanup */
    free_xpquery(p_xpquery);
    return(0);
}

#endif
