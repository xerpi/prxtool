/***************************************************************
 * PRXTool : Utility for PSP executables.
 * (c) TyRaNiD 2k5
 *
 * NidMgr.C - Implementation of a class to manipulate a list
 * of NID Libraries.
 ***************************************************************/

#include <stdlib.h>
#include <jansson.h>
#include <tinyxml/tinyxml.h>
#include "yamltree.h"
#include "yamltreeutil.h"
#include "output.h"
#include "NidMgr.h"
#include "vita-import.h"
#include "prxtypes.h"

struct SyslibEntry
{
	unsigned int nid;
	const char *name;
};

static SyslibEntry g_syslib[] = {
	{ 0x70FBA1E7, "module_process_param" },
	{ 0x6C2224BA, "module_info" },
	{ 0x935CD196, "module_start" },
	{ 0x79F8E492, "module_stop" },
	{ 0x913482A9, "module_exit" },
};

#define MASTER_NID_MAPPER "MasterNidMapper"

/* Default constructor */
CNidMgr::CNidMgr()
	: m_pLibHead(NULL), m_pMasterNids(NULL)
{
}

/* Destructor */
CNidMgr::~CNidMgr()
{
	FreeMemory();
}

/* Free allocated memory */
void CNidMgr::FreeMemory()
{
	LibraryEntry* pLib;

	pLib = m_pLibHead;
	while(pLib != NULL)
	{
		LibraryEntry* pNext;

		pNext = pLib->pNext;

		if(pLib->pNids != NULL)
		{
			delete pLib->pNids;
			pLib->pNids = NULL;
		}

		delete pLib;
		pLib = pNext;
	}

	m_pLibHead = NULL;

	for(unsigned int i = 0; i < m_funcMap.size(); i++)
	{
		FunctionType *p;
		p = m_funcMap[i];
		if(p)
		{
			delete p;
		}
	}
}

/* Generate a simple name based on the library and the nid */
const char *CNidMgr::GenName(const char *lib, u32 nid)
{
	if(lib == NULL)
	{
		snprintf(m_szCurrName, LIB_SYMBOL_NAME_MAX, "syslib_%08X", nid);
	}
	else
	{
		snprintf(m_szCurrName, LIB_SYMBOL_NAME_MAX, "%s_%08X", lib, nid);
	}

	return m_szCurrName;
}

/* Search the NID list for a function and return the name */
const char *CNidMgr::SearchLibs(const char *lib, u32 nid)
{
	const char *pName = NULL;
	LibraryEntry *pLib;

	if(m_pMasterNids)
	{
		pLib = m_pMasterNids;
	}
	else
	{
		pLib = m_pLibHead;
	}

	/* Very lazy, could be sped up using a hash table */
	while(pLib != NULL)
	{
		if((strcmp(lib, pLib->lib_name) == 0) || (m_pMasterNids))
		{
			int iNidLoop;

			for(iNidLoop = 0; iNidLoop < pLib->entry_count; iNidLoop++)
			{
				if(pLib->pNids[iNidLoop].nid == nid)
				{
					pName = pLib->pNids[iNidLoop].name;
					COutput::Printf(LEVEL_DEBUG, "Using %s, nid %08X\n", pName, nid);
					break;
				}
			}

			if(pName != NULL)
			{
				break;
			}
		}

		if(m_pMasterNids)
		{
			pLib = NULL;
		}
		else
		{
			pLib = pLib->pNext;
		}
	}

	if(pName == NULL)
	{
		/* First check special case system library stuff */
		if(strcmp(lib, PSP_SYSTEM_EXPORT) == 0)
		{
			int size;
			int i;

			size = sizeof(g_syslib) / sizeof(SyslibEntry);
			for(i = 0; i < size; i++)
			{
				if(nid == g_syslib[i].nid)
				{
					pName = g_syslib[i].name;
					break;
				}
			}
		}

		if(pName == NULL)
		{
			COutput::Puts(LEVEL_DEBUG, "Using default name");
			pName = GenName(lib, nid);
		}
	}

	return pName;
}

/* Read the NID data from the XML file */
const char* CNidMgr::ReadNid(TiXmlElement *pElement, u32 &nid)
{
	TiXmlHandle nidHandle(pElement);
	TiXmlText *pNid;
	TiXmlText *pName;
	const char* szName;

	szName = NULL;
	pNid = nidHandle.FirstChild("NID").FirstChild().Text();
	pName = nidHandle.FirstChild("NAME").FirstChild().Text();

	if((pNid != NULL) && (pName != NULL))
	{
		nid = strtoul(pNid->Value(), NULL, 16);
		szName = pName->Value();
	}

	return szName;
}

/* Count the number of nids in the current element */
int CNidMgr::CountNids(TiXmlElement *pElement, const char *name)
{
	TiXmlElement *pIterator;
	u32 nid;
	int iCount = 0;

	pIterator = pElement;
	while(pIterator != NULL)
	{
		if(ReadNid(pIterator, nid) != NULL)
		{
			iCount++;
		}
		pIterator = pIterator->NextSiblingElement(name);
	}

	return iCount;
}

/* Process a library XML element */
void CNidMgr::ProcessLibrary(TiXmlElement *pLibrary, const char *prx_name, const char *prx)
{
	TiXmlHandle libHandle(pLibrary);
	TiXmlText *elmName;
	TiXmlText *elmFlags;
	TiXmlElement *elmFunction;
	TiXmlElement *elmVariable;
	int fCount;
	int vCount;
	bool blMasterNids = false;

	assert(prx_name != NULL);
	assert(prx != NULL);

	elmName = libHandle.FirstChild("NAME").FirstChild().Text();
	elmFlags = libHandle.FirstChild("FLAGS").FirstChild().Text();
	if(elmName)
	{
		LibraryEntry *pLib;

		COutput::Printf(LEVEL_DEBUG, "Library %s\n", elmName->Value());
		SAFE_ALLOC(pLib, LibraryEntry);
		if(pLib != NULL)
		{
			memset(pLib, 0, sizeof(LibraryEntry));
			strcpy(pLib->lib_name, elmName->Value());
			if(strcmp(pLib->lib_name, MASTER_NID_MAPPER) == 0)
			{
				blMasterNids = true;
				COutput::Printf(LEVEL_DEBUG, "Found master NID table\n");
			}

			if(elmFlags)
			{
				pLib->flags = strtoul(elmFlags->Value(), NULL, 16);
			}

			strcpy(pLib->prx_name, prx_name);
			strcpy(pLib->prx, prx);
			elmFunction = libHandle.FirstChild("FUNCTIONS").FirstChild("FUNCTION").Element();
			elmVariable = libHandle.FirstChild("VARIABLES").FirstChild("VARIABLE").Element();
			fCount = CountNids(elmFunction, "FUNCTION");
			vCount = CountNids(elmVariable, "VARIABLE");
			pLib->vcount = vCount;
			pLib->fcount = fCount;
			if((fCount+vCount) > 0)
			{
				SAFE_ALLOC(pLib->pNids, LibraryNid[vCount+fCount]);
				if(pLib->pNids != NULL)
				{
					int iLoop;
					const char *pName;

					memset(pLib->pNids, 0, sizeof(LibraryNid) * (vCount+fCount));
					pLib->entry_count = vCount + fCount;
					iLoop = 0;
					while(elmFunction != NULL)
					{
						pName = ReadNid(elmFunction, pLib->pNids[iLoop].nid);
						if(pName)
						{
							pLib->pNids[iLoop].pParentLib = pLib;
							strcpy(pLib->pNids[iLoop].name, pName);
							COutput::Printf(LEVEL_DEBUG, "Read func:%s nid:0x%08X\n", pLib->pNids[iLoop].name, pLib->pNids[iLoop].nid);
							iLoop++;
						}

						elmFunction = elmFunction->NextSiblingElement("FUNCTION");
					}

					while(elmVariable != NULL)
					{
						pName = ReadNid(elmVariable, pLib->pNids[iLoop].nid);
						if(pName)
						{
							strcpy(pLib->pNids[iLoop].name, pName);
							COutput::Printf(LEVEL_DEBUG, "Read var:%s nid:0x%08X\n", pLib->pNids[iLoop].name, pLib->pNids[iLoop].nid);
							iLoop++;
						}

						elmVariable = elmVariable->NextSiblingElement("VARIABLE");
					}
				}
			}

			/* Link into list */
			if(m_pLibHead == NULL)
			{
				m_pLibHead = pLib;
			}
			else
			{
				pLib->pNext = m_pLibHead;
				m_pLibHead = pLib;
			}

			if(blMasterNids)
			{
				m_pMasterNids = pLib;
			}
		}

		/* Allocate library memory */
	}
}

/* Process a PRXFILE XML element */
void CNidMgr::ProcessPrxfile(TiXmlElement *pPrxfile)
{
	TiXmlHandle prxHandle(pPrxfile);
	TiXmlElement *elmLibrary;
	TiXmlText *txtName;
	TiXmlText *txtPrx;
	const char *szPrx;

	txtPrx = prxHandle.FirstChild("PRX").FirstChild().Text();
	txtName = prxHandle.FirstChild("PRXNAME").FirstChild().Text();

	elmLibrary = prxHandle.FirstChild("LIBRARIES").FirstChild("LIBRARY").Element();
	while(elmLibrary)
	{
		COutput::Puts(LEVEL_DEBUG, "Found LIBRARY");

		if(txtPrx == NULL)
		{
			szPrx = "unknown.prx";
		}
		else
		{
			szPrx = txtPrx->Value();
		}

		if(txtName != NULL)
		{
			ProcessLibrary(elmLibrary, txtName->Value(), szPrx);
		}

		elmLibrary = elmLibrary->NextSiblingElement("LIBRARY");
	}
}

/* Add an XML file to the current library list */
bool CNidMgr::AddXmlFile(const char *szFilename)
{
	TiXmlDocument doc(szFilename);
	bool blRet = false;

	if(doc.LoadFile())
	{
		COutput::Printf(LEVEL_DEBUG, "Loaded XML file %s", szFilename);
		TiXmlHandle docHandle(&doc);
		TiXmlElement *elmPrxfile;

		elmPrxfile = docHandle.FirstChild("PSPLIBDOC").FirstChild("PRXFILES").FirstChild("PRXFILE").Element();
		while(elmPrxfile)
		{
			COutput::Puts(LEVEL_DEBUG, "Found PRXFILE");
			ProcessPrxfile(elmPrxfile);

			elmPrxfile = elmPrxfile->NextSiblingElement("PRXFILE");
		}
		blRet = true;
	}
	else
	{
		COutput::Printf(LEVEL_ERROR, "Couldn't load xml file %s\n", szFilename);
	}

	return blRet;
}

bool CNidMgr::vita_imports_load_json(FILE *text, int verbose)
{
	bool blMasterNids = false;

	json_t *libs, *lib_data;
	json_error_t error;
	const char *lib_name, *mod_name, *target_name;

	libs = json_loadf(text, 0, &error);
	if (libs == NULL) {
		COutput::Printf(LEVEL_ERROR, "error: on line %d: %s\n", error.line, error.text);
		return false;
	}

	if (!json_is_object(libs)) {
		COutput::Printf(LEVEL_ERROR, "error: modules is not an object\n");
		json_decref(libs);
		return false;
	}

	int i, j, k;

	i = -1;
	json_object_foreach(libs, lib_name, lib_data) {
		json_t *nid, *modules, *mod_data;

		i++;

		if (!json_is_object(lib_data)) {
			COutput::Printf(LEVEL_ERROR, "error: library %s is not an object\n", lib_name);
			json_decref(libs);
			return false;
		}

		nid = json_object_get(lib_data, "nid");
		if (!json_is_integer(nid)) {
			COutput::Printf(LEVEL_ERROR, "error: library %s: nid is not an integer\n", lib_name);
			json_decref(libs);
			return false;
		}

		modules = json_object_get(lib_data, "modules");
		if (!json_is_object(modules)) {
			COutput::Printf(LEVEL_ERROR, "error: library %s: module is not an object\n", lib_name);
			json_decref(libs);
			return false;
		}

		j = -1;
		json_object_foreach(modules, mod_name, mod_data) {
			json_t *nid, *kernel, *functions, *variables, *target_nid;
			int has_variables = 1;

			j++;

			if (!json_is_object(mod_data)) {
				COutput::Printf(LEVEL_ERROR, "error: module %s is not an object\n", mod_name);
				json_decref(libs);
				return false;
			}

			nid = json_object_get(mod_data, "nid");
			if (!json_is_integer(nid)) {
				COutput::Printf(LEVEL_ERROR, "error: module %s: nid is not an integer\n", mod_name);
				json_decref(libs);
				return false;
			}

			kernel = json_object_get(mod_data, "kernel");
			if (!json_is_boolean(kernel)) {
				COutput::Printf(LEVEL_ERROR, "error: module %s: kernel is not a boolean\n", mod_name);
				json_decref(libs);
				return false;
			}

			functions = json_object_get(mod_data, "functions");
			if (!json_is_object(functions)) {
				COutput::Printf(LEVEL_ERROR, "error: module %s: functions is not an array\n", mod_name);
				json_decref(libs);
				return false;
			}

			variables = json_object_get(mod_data, "variables");
			if (variables == 0) {
				has_variables = 0;
			}

			if (has_variables && !json_is_object(variables)) {
				COutput::Printf(LEVEL_ERROR, "error: module %s: variables is not an array\n", mod_name);
				json_decref(libs);
				return false;
			}

			LibraryEntry *pLib;

			COutput::Printf(LEVEL_DEBUG, "Library %s\n", mod_name);
			SAFE_ALLOC(pLib, LibraryEntry);
			if(pLib != NULL)
			{
				memset(pLib, 0, sizeof(LibraryEntry));
				strcpy(pLib->lib_name, mod_name);
				strcpy(pLib->prx_name, mod_name);
				strcpy(pLib->prx, mod_name);
				if(strcmp(pLib->lib_name, MASTER_NID_MAPPER) == 0)
				{
					blMasterNids = true;
					COutput::Printf(LEVEL_DEBUG, "Found master NID table\n");
				}

				int fCount = json_object_size(functions);
				int vCount = json_object_size(variables);

				pLib->vcount = vCount;
				pLib->fcount = fCount;
				if((fCount+vCount) > 0)
				{
					SAFE_ALLOC(pLib->pNids, LibraryNid[vCount+fCount]);
					if(pLib->pNids != NULL)
					{
						int iLoop = 0;
						const char *pName;

						memset(pLib->pNids, 0, sizeof(LibraryNid) * (vCount+fCount));
						pLib->entry_count = vCount + fCount;

						k = -1;
						json_object_foreach(functions, target_name, target_nid) {
							k++;

							if (!json_is_integer(target_nid)) {
								COutput::Printf(LEVEL_ERROR, "error: function %s: nid is not an integer\n", target_name);
								json_decref(libs);
								return false;
							}

							pLib->pNids[iLoop].pParentLib = pLib;
							pLib->pNids[iLoop].nid = json_integer_value(target_nid);
							strcpy(pLib->pNids[iLoop].name, target_name);
							COutput::Printf(LEVEL_DEBUG, "Read func:%s nid:0x%08X\n", pLib->pNids[iLoop].name, pLib->pNids[iLoop].nid);
							iLoop++;
						}

						if (has_variables) {
							k = -1;
							json_object_foreach(variables, target_name, target_nid) {
								k++;

								if (!json_is_integer(target_nid)) {
									COutput::Printf(LEVEL_ERROR, "error: variable %s: nid is not an integer\n", target_name);
									json_decref(libs);
									return false;
								}

								pLib->pNids[iLoop].pParentLib = pLib;
								pLib->pNids[iLoop].nid = json_integer_value(target_nid);
								strcpy(pLib->pNids[iLoop].name, target_name);
								iLoop++;
							}
						}
					}
				}

				if(m_pLibHead == NULL)
				{
					m_pLibHead = pLib;
				}
				else
				{
					pLib->pNext = m_pLibHead;
					m_pLibHead = pLib;
				}

				if(blMasterNids)
				{
					m_pMasterNids = pLib;
				}
			}
		}
	}

	return true;
}

int process_import_functions(yaml_node *parent, yaml_node *child, vita_imports_module_t *library) {
	if (!is_scalar(parent)) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting function to be scalar, got '%s'.\n"
			, parent->position.line
			, parent->position.column
			, node_type_str(parent));

		return -1;
	}

	yaml_scalar *key = &parent->data.scalar;

	// create an export symbol for this function
	vita_imports_stub_t *symbol = vita_imports_stub_new(key->value,0);

	if (!is_scalar(child)) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting function value to be scalar, got '%s'.\n"
			, child->position.line
			, child->position.column
			, node_type_str(child));

		return -1;
	}

	if (process_32bit_integer(child, &symbol->NID) < 0) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, could not convert function nid '%s' to 32 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
		return -1;
	}
	// append to list
	library->functions = (vita_imports_stub_t**)realloc(library->functions, (library->n_functions+1)*sizeof(vita_imports_stub_t*));
	library->functions[library->n_functions++] = symbol;

	return 0;
}

int process_import_variables(yaml_node *parent, yaml_node *child, vita_imports_module_t *library) {
	if (!is_scalar(parent)) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting variable to be scalar, got '%s'.\n"
			, parent->position.line
			, parent->position.column
			, node_type_str(parent));

		return -1;
	}

	yaml_scalar *key = &parent->data.scalar;

	// create an export symbol for this variable
	vita_imports_stub_t *symbol = vita_imports_stub_new(key->value,0);

	if (!is_scalar(child)) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting variable value to be scalar, got '%s'.\n"
			, child->position.line
			, child->position.column
			, node_type_str(child));

		return -1;
	}

	if (process_32bit_integer(child, &symbol->NID) < 0) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, could not convert variable nid '%s' to 32 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
		return -1;
	}
	// append to list
	library->variables = (vita_imports_stub_t**)realloc(library->variables, (library->n_variables+1)*sizeof(vita_imports_stub_t*));
	library->variables[library->n_variables++] = symbol;

	return 0;
}

int process_library(yaml_node *parent, yaml_node *child, vita_imports_module_t *library) {
	if (!is_scalar(parent)) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting library key to be scalar, got '%s'.\n", parent->position.line, parent->position.column, node_type_str(parent));
		return -1;
	}

	yaml_scalar *key = &parent->data.scalar;

	if (strcmp(key->value, "kernel") == 0) {
		if (!is_scalar(child)) {
			COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting library syscall flag to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}

		if (process_bool(child, &library->is_kernel) < 0) {
			COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, could not convert library flag to boolean, got '%s'. expected 'true' or 'false'.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
	}
	else if (strcmp(key->value, "functions") == 0) {
		if (yaml_iterate_mapping(child, (mapping_functor)process_import_functions, library) < 0)
			return -1;
	}
	else if (strcmp(key->value, "variables") == 0) {
		if (yaml_iterate_mapping(child, (mapping_functor)process_import_variables, library) < 0)
			return -1;
	}
	else if (strcmp(key->value, "nid") == 0) {
		if (!is_scalar(child)) {
			COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting library nid to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}

		if (process_32bit_integer(child, &library->NID) < 0) {
			COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, could not convert library nid '%s' to 32 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
	}
	else {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, unrecognised library key '%s'.\n", child->position.line, child->position.column, key->value);
		return -1;
	}

	return 0;
}

int process_libraries(yaml_node *parent, yaml_node *child, vita_imports_lib_t *import) {
	if (!is_scalar(parent)) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting library key to be scalar, got '%s'.\n", parent->position.line, parent->position.column, node_type_str(parent));
		return -1;
	}

	yaml_scalar *key = &parent->data.scalar;

	vita_imports_module_t *library = vita_imports_module_new("",false,0,0,0);

	// default values
	library->name = strdup(key->value);

	if (yaml_iterate_mapping(child, (mapping_functor)process_library, library) < 0)
		return -1;

	import->modules = (vita_imports_module_t**)realloc(import->modules, (import->n_modules+1)*sizeof(vita_imports_module_t*));
	import->modules[import->n_modules++] = library;

	return 0;
}

int process_import(yaml_node *parent, yaml_node *child, vita_imports_lib_t *import) {
	if (!is_scalar(parent)) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting module key to be scalar, got '%s'.\n", parent->position.line, parent->position.column, node_type_str(parent));
		return -1;
	}

	yaml_scalar *key = &parent->data.scalar;

	if (strcmp(key->value, "nid") == 0) {
		if (!is_scalar(child)) {
			COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting module nid to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}

		if (process_32bit_integer(child, &import->NID) < 0) {
			COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, could not convert module nid '%s' to 32 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
	}
	else if (strcmp(key->value, "libraries") == 0) {

		if (yaml_iterate_mapping(child, (mapping_functor)process_libraries, import) < 0)
			return -1;

	}
	else {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, unrecognised module key '%s'.\n", child->position.line, child->position.column, key->value);
		return -1;
	}

	return 0;
}

int process_import_list(yaml_node *parent, yaml_node *child, vita_imports_t *imports) {
	if (!is_scalar(parent)) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting modules key to be scalar, got '%s'.\n", parent->position.line, parent->position.column, node_type_str(parent));
		return -1;
	}

	yaml_scalar *key = &parent->data.scalar;

	vita_imports_lib_t *import = vita_imports_lib_new(key->value,0,0);

	if (yaml_iterate_mapping(child, (mapping_functor)process_import, import) < 0)
		return -1;

	imports->libs = (vita_imports_lib_t**)realloc(imports->libs, (imports->n_libs+1)*sizeof(vita_imports_lib_t*));
	imports->libs[imports->n_libs++] = import;
	return 0;
}

bool CNidMgr::read_vita_imports_yml(yaml_document *doc)
{
	bool blMasterNids = false;

	if (!is_mapping(doc)) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting root node to be a mapping, got '%s'.\n", doc->position.line, doc->position.column, node_type_str(doc));
		return false;
	}

	yaml_mapping *root = &doc->data.mapping;

	// check we only have one entry
	if (root->count < 1) {
		COutput::Printf(LEVEL_ERROR, "error: line: %zd, column: %zd, expecting at least one entry within root mapping, got %zd.\n", doc->position.line, doc->position.column, root->count);
		return false;
	}

	vita_imports_t *imports  = vita_imports_new(0);
	if (imports == NULL)
		return false;

	for(int n = 0; n < root->count; n++){
		// check lhs is a scalar
		if (is_scalar(root->pairs[n]->lhs)) {

			if (strcmp(root->pairs[n]->lhs->data.scalar.value, "modules")==0) {
				if (yaml_iterate_mapping(root->pairs[n]->rhs, (mapping_functor)process_import_list, imports) < 0)
					return false;
				continue;
			}

			COutput::Printf(LEVEL_WARNING, "warning: line: %zd, column: %zd, unknow tag '%s'.\n", root->pairs[n]->lhs->position.line, root->pairs[n]->lhs->position.column, root->pairs[n]->lhs->data.scalar.value);

		}

	}

	/* Add imports to the NID list */
	for (int i = 0; i < imports->n_libs; i++) {
		vita_imports_lib_t *lib = imports->libs[i];

		for (int j = 0; j < lib->n_modules; j++) {
			vita_imports_module_t *mod = lib->modules[j];
			int fCount = mod->n_functions;
			int vCount = mod->n_variables;

			LibraryEntry *pLib;
			SAFE_ALLOC(pLib, LibraryEntry);
			if (!pLib)
				continue;

			memset(pLib, 0, sizeof(LibraryEntry));
			strcpy(pLib->lib_name, mod->name);
			strcpy(pLib->prx_name, mod->name);
			strcpy(pLib->prx, mod->name);
			pLib->vcount = vCount;
			pLib->fcount = fCount;
			pLib->entry_count = vCount + fCount;

			if(strcmp(pLib->lib_name, MASTER_NID_MAPPER) == 0) {
				blMasterNids = true;
				COutput::Printf(LEVEL_DEBUG, "Found master NID table\n");
			}

			SAFE_ALLOC(pLib->pNids, LibraryNid[vCount+fCount]);
			if (!pLib->pNids)
				continue;

			memset(pLib->pNids, 0, sizeof(LibraryNid) * (vCount+fCount));

			int iLoop;

			/* Functions */
			iLoop = 0;
			for (int k = 0; k < fCount; k++) {
				vita_imports_stub_t *func = mod->functions[k];

				pLib->pNids[iLoop].pParentLib = pLib;
				pLib->pNids[iLoop].nid = func->NID;
				strcpy(pLib->pNids[iLoop].name, func->name);

				iLoop++;
			}

			/* Variables */
			for (int k = 0; k < vCount; k++) {
				vita_imports_stub_t *var = mod->variables[k];

				pLib->pNids[iLoop].pParentLib = pLib;
				pLib->pNids[iLoop].nid = var->NID;
				strcpy(pLib->pNids[iLoop].name, var->name);

				iLoop++;
			}

			if (m_pLibHead == NULL) {
				m_pLibHead = pLib;
			} else {
				pLib->pNext = m_pLibHead;
				m_pLibHead = pLib;
			}

			if (blMasterNids) {
				m_pMasterNids = pLib;
			}
		}
	}

	vita_imports_free(imports);

	return true;

}

bool CNidMgr::vita_imports_load_yml(FILE *text, int verbose)
{
	uint32_t nid = 0;
	yaml_error error = {0};

	yaml_tree *tree = parse_yaml_stream(text, &error);

	if (!tree)
	{
		COutput::Printf(LEVEL_ERROR, "error: %s\n", error.problem);
		free(error.problem);
		return false;
	}

	if (tree->count != 1)
	{
		COutput::Printf(LEVEL_ERROR, "error: expecting a single yaml document, got: %zd\n", tree->count);
		// TODO: cleanup tree
		return false;
	}

	return read_vita_imports_yml(tree->docs[0]);
}

bool CNidMgr::AddNIDFile(const char *szFilename)
{
	FILE *fp;
	bool ret;
	const char *dot = strrchr(szFilename, '.');

	if (!dot) {
		COutput::Printf(LEVEL_ERROR, "Error: unknown NID file type %s\n", szFilename);
		return false;
	}

	fp = fopen(szFilename, "r");
	if (fp == NULL) {
		COutput::Printf(LEVEL_ERROR, "Error: could not open %s\n", szFilename);
		return false;
	}

	if (!strcmp(dot + 1, "xml")) {
		ret = AddXmlFile(szFilename);
	} else if (!strcmp(dot + 1, "json")) {
		ret = vita_imports_load_json(fp, 1);
	} else if (!strcmp(dot + 1, "yml")) {
		ret = vita_imports_load_yml(fp, 1);
	} else {
		COutput::Printf(LEVEL_ERROR, "Error: unknown NID file type %s\n", szFilename);
		ret = false;
	}

	fclose(fp);

	return ret;
}

/* Find the name based on our list of names */
const char *CNidMgr::FindLibName(const char *lib, u32 nid)
{
	return SearchLibs(lib, nid);
}

LibraryEntry *CNidMgr::GetLibraries(void)
{
	return m_pLibHead;
}

/* Find the name of the dependany library for a specified lib */
const char *CNidMgr::FindDependancy(const char *lib)
{
	LibraryEntry *pLib;

	pLib = m_pLibHead;

	while(pLib != NULL)
	{
		if(strcmp(pLib->lib_name, lib) == 0)
		{
			return pLib->prx;
		}

		pLib = pLib->pNext;
	}

	return NULL;
}

static char *strip_whitesp(char *str)
{
	int len;

	while(isspace(*str))
	{
		str++;
	}

	len = strlen(str);
	while((len > 0) && (isspace(str[len-1])))
	{
		str[len-1] = 0;
		len--;
	}

	if(len == 0)
	{
		return NULL;
	}

	return str;
}

bool CNidMgr::AddFunctionFile(const char *szFilename)
{
	FILE *fp;

	fp = fopen(szFilename, "r");
	if(fp)
	{
		char line[1024];

		while(fgets(line, sizeof(line), fp))
		{
			char *name;
			char *args = NULL;
			char *ret = NULL;

			name = strip_whitesp(line);
			if(name == NULL)
			{
				continue;
			}

			args = strchr(name, '|');
			if(args)
			{
				*args++ = 0;
				ret = strchr(args, '|');
				if(ret)
				{
					*ret++ = 0;
				}
			}

			if((name) && (name[0] != '#'))
			{
				FunctionType *p = new FunctionType;

				memset(p, 0, sizeof(FunctionType));
				snprintf(p->name, FUNCTION_NAME_MAX, "%s", name);
				if(args)
				{
					snprintf(p->args, FUNCTION_ARGS_MAX, "%s", args);
				}
				if(ret)
				{
					snprintf(p->ret, FUNCTION_RET_MAX, "%s", ret);
				}
				m_funcMap.insert(m_funcMap.end(), p);
				COutput::Printf(LEVEL_DEBUG, "Function: %s %s(%s)\n", p->ret, p->name, p->args);
			}
		}
		fclose(fp);
		return true;
	}

	return false;
}

FunctionType *CNidMgr::FindFunctionType(const char *name)
{
	FunctionType *ret = NULL;

	for(unsigned int i = 0; i < m_funcMap.size(); i++)
	{
		FunctionType *p = NULL;
		p = m_funcMap[i];
		if((p) && (strcmp(name, p->name) == 0))
		{
			ret = p;
			break;
		}
	}

	return ret;
}
