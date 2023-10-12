// PgServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <sys/types.h>
//#include <sys/select.h>
//#include <sys/socket.h>
#include <microhttpd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <vector>
#include <windows.h>
#include <direct.h>

#define INITIALIZE_A3D_API

#include "PgVisitTesselation.h"
#include "VisitorTesselation.h"
#include "TreeTraverse.h"

//#include "A3DSDKLicenseKey.h"
#include "hoops_license.h"

#include "pgapi.h"
#include "pgkey.h"

#define PORT 8888

//#define FILE_NAME ".\\data\\Ring.CATPart"
//char* FILE_NAME = ".\\data\\Bunny.stl";
//#define FILE_NAME ".\\data\\Laurana50k.stl"
//char* FILE_NAME = ".\\temp.stl";

#define CHECK_RET { if (iRet!=A3D_SUCCESS) return iRet; }


int faceCount = 0;
float fHollowAmount = 1;
int nSimplifyAmount = 50;
char filename[1024];

struct session
{
	PTSolid solid;
	int faceCount;
	int sessionId;
};

struct Point
{
	float x, y, z;
};

class MeshBuilder
{
public:
	std::vector<Point> points;
	std::vector<int> faces;
};

int print_out_key(void *cls, enum MHD_ValueKind kind,
	const char *key, const char *value)
{
	printf("%s: %s\n", key, value);
	return MHD_YES;
}

PTEnvironment pg_environment;
PTSolid solid = NULL;

int InitializeExchange()
{
	if (!A3DSDKLoadLibrary(NULL))
	{
		printf("Cannot load library\n");
		return 1;
	}

	A3DInt32 iRet = A3D_SUCCESS;
	//CHECK_RET(A3DLicPutLicense(A3DLicPutLicenseFile, pcCustomerKey, pcVariableKey));
	CHECK_RET(A3DLicPutUnifiedLicense(HOOPS_LICENSE));
	A3DInt32 iMajorVersion = 0, iMinorVersion = 0;
	CHECK_RET(A3DDllGetVersion(&iMajorVersion, &iMinorVersion));
	CHECK_RET(A3DDllInitialize(A3D_DLL_MAJORVERSION, A3D_DLL_MINORVERSION));

	printf("HOOPS Exchanged Loaded\n");

	return iRet;
}

static void handle_pg_error(PTStatus status, char *err_string)
{
	PTStatus status_code;
	PTStatus err_code;
	PTStatus func_code;
	PTStatus fatal_error = PV_STATUS_OK;
	/* The status is made up of 3 parts */
	status_code = PM_STATUS_FROM_API_ERROR_CODE(status);
	func_code = PM_FN_FROM_API_ERROR_CODE(status);
	err_code = PM_ERR_FROM_API_ERROR_CODE(status);
	if (status_code & PV_STATUS_BAD_CALL)
	{
		printf("PG:BAD_CALL: Function %d Error %d: %s\n", func_code, err_code, err_string);
	}
	if (status_code & PV_STATUS_MEMORY)
	{
		printf("PG:MEMORY: Function %d Error %d: %s\n", func_code, err_code, err_string);
		fatal_error |= status;
	}
	if (status_code & PV_STATUS_EXCEPTION)
	{
		printf("PG:EXCEPTION: Function %d Error %d: %s\n", func_code, err_code, err_string);
		fatal_error |= status;
	}
	if (status_code & PV_STATUS_FILE_IO)
	{
		printf("PG:FILE I/0: Function %d Error %d: %s\n", func_code, err_code, err_string);
	}
	if (status_code & PV_STATUS_INTERRUPT)
	{
		printf("PG:INTERRUPT: Function %d Error %d: %s\n", func_code, err_code, err_string);
	}
	if (status_code & PV_STATUS_INTERNAL_ERROR)
	{
		printf("PG:INTERNAL_ERROR: Function %d Error %d: %s\n", func_code, err_code, err_string);
		fatal_error |= status;
	}
}

int InitializePolygonica()
{
	PTStatus status = PFInitialise(PV_LICENSE, NULL);
	if (status) {
		printf("Polygonica failed to initialize.\n");
		return status;
	}

	status = PFEnvironmentCreate(NULL, &pg_environment);
	if (status) {
		printf("Failed to create Polygonica environment.");
		return status;
	}

	PFEntitySetPointerProperty(pg_environment, PV_ENV_PROP_ERROR_REPORT_CB, handle_pg_error);

	printf("Polygonica Loaded.\n");
	return status;
}

bool TesselationToSolid(PgVisitTesselation *psPgVisitTesselation, PTSolid *pSolid)
{
	size_t faceCount = psPgVisitTesselation->GetPointsSize() / 3; // pgArgs->shellKey.GetFaceCount();
	PTNat32* pg_faces = new PTNat32[faceCount * 3];
	PTStatus status = PFThreadRegister();

	int progress = 0;

	for (int i = 0; i < faceCount*3; i++)
	{
		pg_faces[i] = i;
	}

	PTPoint* vertices = new PTPoint[psPgVisitTesselation->GetPointsSize()];

	progress = 0;

	for (int i = 0; i < psPgVisitTesselation->GetPointsSize(); i++)
	{
		vertices[i][0] = psPgVisitTesselation->GetPoints()[i].x;
		vertices[i][1] = psPgVisitTesselation->GetPoints()[i].y;
		vertices[i][2] = psPgVisitTesselation->GetPoints()[i].z;
	}

	status = PFSolidCreateFromMesh(
		pg_environment,
		faceCount,	// total number of triangles
		NULL,		// no internal loops
		NULL,		// all faces are triangles
		pg_faces,	// indices into vertex array
		vertices,		// pointer to vertex array
		NULL,		// no options
		pSolid		// resultant pg solid
	);

	PTSolidMergeVerticesOpts options;
	PMInitSolidMergeVerticesOpts(&options);
	status = PFSolidMergeVertices(solid, &options);

	status = PFThreadUnregister();
	return status;
}

int LoadFile(char* file_name, PgVisitTesselation *psPgVisitTesselation)
{
	A3DInt32 iRet = A3D_SUCCESS;

	strcpy(filename, file_name);

	/////////////////////////////////////////////
	// 1- Load CAD file
	/////////////////////////////////////////////
	A3DRWParamsLoadData sReadParam;
	A3D_INITIALIZE_DATA(A3DRWParamsLoadData, sReadParam);
	sReadParam.m_sGeneral.m_bReadSolids = true;
	sReadParam.m_sGeneral.m_eReadGeomTessMode = kA3DReadGeomAndTess;
	sReadParam.m_sTessellation.m_eTessellationLevelOfDetail = kA3DTessLODExtraHigh;

	A3DAsmModelFile* pModelFile = NULL;
	//iRet = A3DAsmModelFileLoadFromFile(acCADFileUTF8, &sReadParam, &pModelFile);
	iRet = A3DAsmModelFileLoadFromFile(file_name, &sReadParam, &pModelFile);
	if (iRet != A3D_SUCCESS && iRet != A3D_LOAD_MISSING_COMPONENTS)
		return -1;

	/////////////////////////////////////////////
	// Traverse Assembly Tree Structure
	/////////////////////////////////////////////
	A3DAsmModelFileData sModelFileData;
	A3D_INITIALIZE_DATA(A3DAsmModelFileData, sModelFileData);
	CHECK_RET(A3DAsmModelFileGet(pModelFile, &sModelFileData));

	for (A3DUns32 uI = 0; uI < sModelFileData.m_uiPOccurrencesSize; uI++)
	{
		A3DProductOccurenceConnector sTreeConnector(sModelFileData.m_ppPOccurrences[uI]);
		CHECK_RET(sTreeConnector.TraversePO(sModelFileData.m_ppPOccurrences[uI], psPgVisitTesselation));
	}

	CHECK_RET(A3DAsmModelFileGet(NULL, &sModelFileData));
	return 0;
}

std::vector<float> GetOpenEdges(PTSolid *pSolid)
{
	PTStatus status = PFThreadRegister();

	/// FIND OPEN EDGES ///
	PTCategory openEdgesCategory;
	status = PFCategoryCreate(
		pg_environment,
		PV_CRITERION_OPEN_EDGES,
		NULL, // no options necessary
		&openEdgesCategory
	);

	PTEntityGroup openEdgesEntityGroup;
	status = PFEntityGroupCreateFromCategory(
		*pSolid,
		openEdgesCategory,
		&openEdgesEntityGroup
	);

	PTEntityList openEdgeList;
	status = PFEntityCreateEntityList(openEdgesEntityGroup, PV_ENTITY_TYPE_EDGE, NULL, &openEdgeList);

	PTEntity aEntity;
	PTEdge eEdge;
	PTVertex vFrom, vTo;
	PTPoint vFromPos, vToPos;
	int iEntity;

	std::vector<float> floatArray;
	
	if ((aEntity = PFEntityListGetFirst(openEdgeList)))
	{
		iEntity = 0;
		while ((eEdge = (PTEdge)aEntity))
		{
			iEntity++;
			PFEdgeGetVertices(eEdge, &vFrom, &vTo);
			PFEntityGetPointProperty(vFrom, PV_VERTEX_PROP_POINT, vFromPos);
			PFEntityGetPointProperty(vTo, PV_VERTEX_PROP_POINT, vToPos);

			floatArray.push_back(vFromPos[0]);
			floatArray.push_back(vFromPos[1]);
			floatArray.push_back(vFromPos[2]);

			floatArray.push_back(vToPos[0]);
			floatArray.push_back(vToPos[1]);
			floatArray.push_back(vToPos[2]);

			aEntity = PFEntityListGetNext(openEdgeList, aEntity);
		}
	}

	status = PFEntityGroupDestroy(openEdgesEntityGroup);
	status = PFCategoryInvalidate(openEdgesCategory);

	status = PFThreadUnregister();

	return floatArray;
}

std::vector<float> GetEntityListAsFloatArray(PTEntityList face_list)
{
	std::vector<float> floatArray;

	PTStatus status = 0;

	PTEntity aEntity = PFEntityListGetFirst(face_list);

	while (aEntity != NULL)
	{
		PTFace face = aEntity;
		PTFaceEdgeData edgeData;
		PFFaceGetEdges(face, &edgeData);

		for (int i = 0; i < edgeData.num_outer_edges; i++) // should be 3
		{
			PTEdge edge = edgeData.edges[i];
			bool isForward = edgeData.edge_is_forward[i];
			PTVertex vFrom, vTo;
			PTPoint vFromPos, vToPos;
			PFEdgeGetVertices(edge, &vFrom, &vTo);
			PFEntityGetPointProperty(vFrom, PV_VERTEX_PROP_POINT, vFromPos);
			PFEntityGetPointProperty(vTo, PV_VERTEX_PROP_POINT, vToPos);
			if (isForward)
			{
				floatArray.push_back(vFromPos[0]);
				floatArray.push_back(vFromPos[1]);
				floatArray.push_back(vFromPos[2]);
			}
			else
			{
				floatArray.push_back(vToPos[0]);
				floatArray.push_back(vToPos[1]);
				floatArray.push_back(vToPos[2]);
			}
		}

		aEntity = PFEntityListGetNext(face_list, aEntity);
	}

	return floatArray;
}

std::vector<float> GetIntersectingTriangles(PTSolid *pSolid)
{
	PTStatus status = PFThreadRegister();

	std::vector<float> floatArray;

	/// FIND SELF INTERSECTIONS ///
	PTCategory selfIntersectionsCategory;
	status = PFCategoryCreate(
		pg_environment,
		PV_CRITERION_SELF_INT_FACES,
		NULL, // no options necessary
		&selfIntersectionsCategory
	);

	PTEntityGroup selfIntersectionsEntityGroup;
	status = PFEntityGroupCreateFromCategory(
		*pSolid,
		selfIntersectionsCategory,
		&selfIntersectionsEntityGroup
	);

	PTEntityList selfIntersectionsList;
	status = PFEntityCreateEntityList(selfIntersectionsEntityGroup, PV_ENTITY_TYPE_FACE, NULL, &selfIntersectionsList);

	floatArray = GetEntityListAsFloatArray(selfIntersectionsList);

	status = PFEntityGroupDestroy(selfIntersectionsEntityGroup);
	status = PFCategoryInvalidate(selfIntersectionsCategory);

	status = PFThreadUnregister();

	return floatArray;
}

PTStatus AutoHeal(PTSolid solid)
{
	PTStatus status = PFThreadRegister();

	PTBoolean solid_is_closed = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_CLOSED);
	if (!solid_is_closed)
	{
		PTSolidCloseOpts closeOpts;
		PMInitSolidCloseOpts(&closeOpts);

		status = PFSolidClose(solid, &closeOpts);
		solid_is_closed = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_CLOSED);
	}

	PTBoolean solid_has_bad_orientation = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_BAD_ORIENTATION); // not really needed if we are closing the solid, but what if the solid is closed and the orientationis bad? TODO, fix this!
																											// it may be closed, but still have self intersections, so add code here
	if (!solid_has_bad_orientation)
	{

	}

	PTBoolean solid_has_self_intersects = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_SELF_INTERSECTS);
	if (solid_has_self_intersects)
	{
		PTSolidFixSelfIntsOpts selfIntsOpts;
		PMInitSolidFixSelfIntsOpts(&selfIntsOpts);

		status = PFSolidFixSelfIntersections(solid, &selfIntsOpts);
		solid_has_self_intersects = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_SELF_INTERSECTS);
	}

	PTBoolean solid_is_mainfold = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_MANIFOLD);
	if (!solid_is_mainfold)
	{
		PTSolidSetPrecisionOpts options;
		PMInitSolidSetPrecisionOpts(&options);
		options.make_manifold = true;
		PTPrecisionType flags = PV_PREC_TYPE_SINGLE;
		status = PFSolidSetPrecision(solid, flags, &options);
		solid_is_mainfold = PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_MANIFOLD);
	}

	status = PFThreadUnregister();

	return status;
}

PTStatus mesh_begin(PTPointer app_data,
	PTNat32 num_faces,
	PTNat32 num_loops,
	PTNat32 num_vertex_indices,
	PTNat32 num_vertices,
	PTNat32 vertex_array_size,
	PTPoint **vertices,
	PTPointer **vertex_app_data)
{
	//ShellBuilder* shellBuilder = (ShellBuilder*)app_data;
	MeshBuilder *pMeshBuilder = (MeshBuilder *)app_data;

	PTPoint ptPos;
	//shellBuilder->points.resize(num_vertices);
	//shellBuilder->faces.resize(num_faces * 4);
	//shellBuilder->current_face_index = 0;
	//shellBuilder->progress = 0;

	for (PTNat32 n = 0; n < num_vertices; n++) // could we just copy memory from **vertices?
	{
		PFMeshGetVertexPosition(ptPos, vertices, vertex_array_size, n);
		//shellBuilder->points[n] = HPS::Point(, (float)ptPos[1], (float)ptPos[2]);
		Point pt;
		pt.x = (float)ptPos[0];
		pt.y = (float)ptPos[1];
		pt.z = (float)ptPos[2];
		pMeshBuilder->points.push_back(pt);		
	}

	return PV_STATUS_OK;
};

PTStatus mesh_add_triangle(PTMeshPolygon *polygon)
{
	MeshBuilder* pMeshBuilder = (MeshBuilder*)polygon->options->app_data;

	//int current_progress = (shellBuilder->current_face_index * 100) / shellBuilder->faces.size();
	//if (current_progress > shellBuilder->progress)
	//{
	//	shellBuilder->pCUPDUPData->SetProgress(_T("Creating shell"), current_progress);
	//	shellBuilder->progress = current_progress;
	//}

	//int v0 = polygon->indices[0];
	//int v1 = polygon->indices[1];
	//int v2 = polygon->indices[2];

	//shellBuilder->faces[shellBuilder->current_face_index] = 3;
	//shellBuilder->faces[shellBuilder->current_face_index + 1] = v0;
	//shellBuilder->faces[shellBuilder->current_face_index + 2] = v1;
	//shellBuilder->faces[shellBuilder->current_face_index + 3] = v2;
	//shellBuilder->current_face_index += 4;

	pMeshBuilder->faces.push_back(polygon->indices[0]);
	pMeshBuilder->faces.push_back(polygon->indices[1]);
	pMeshBuilder->faces.push_back(polygon->indices[2]);

	return PV_STATUS_OK;
}

PTStatus mesh_end(PTPointer app_data)
{
	return PV_STATUS_OK;
}

int Simplify(PTSolid solid)
{
	PTStatus status = PFThreadRegister();

	//SimplifyParams* simplifyParams = (SimplifyParams*)pCUPDUPData->GetAppData();

	PTSolidSimplifyOpts options;
	PMInitSolidSimplifyOpts(&options);

	double actual_error = 0.0;

	options.simplify_limits = 0;
	options.simplify_limits |= PV_SIMPLIFY_LIMIT_NUM_FACES;
	options.actual_error = &actual_error;
	options.max_error = 0.1;
	options.min_num_faces = faceCount * (1 - nSimplifyAmount*0.01);//simplifyParams->min_num_faces;
	options.avoid_new_self_isects = false;
	options.type = PV_SIMPLIFY_MIXED;
	
	//options.app_data = (PTPointer)pCUPDUPData;
	//options.progress_callback = progress_cb;

	status = PFSolidSimplify(solid, &options);

	faceCount = faceCount / 2;

	status = PFThreadUnregister();

	return true;
}

int Hollow(PTSolid solid)
{
	PTStatus status = PFThreadRegister();

	PTSolidOffsetOpts options;
	PMInitSolidOffsetOpts(&options);
	options.keep_original_geometry = TRUE;

	/*PTBounds bounds;
	PFEntityGetBoundsProperty(solid, PV_SOLID_PROP_BOUNDS, bounds);

	float dx = bounds[1] - bounds[0]; // xdim
	float dy = bounds[3] - bounds[2]; // ydim
	float dz = bounds[5] - bounds[4]; // zdim
	float maxd = max(dx, max(dy, dz));

	float amount = 0.02 * maxd;*/

	status = PFSolidOffset(solid, -fHollowAmount, fHollowAmount * 0.1, &options);

	status = PFThreadUnregister();

	return true;
}

std::vector<float> SolidToFloatArray(PTSolid solid)
{
	MeshBuilder meshBuilder;

	PTStatus status = PFThreadRegister();

	//ShellBuilder shellBuilder;

	// Get the data from the solid and repopulate the shell!
	PTGetMeshOpts options;
	PMInitGetMeshOpts(&options);
	options.app_data = &meshBuilder;// shellBuilder;
	options.begin_callback = mesh_begin;
	options.add_polygon_callback = &mesh_add_triangle;
	options.end_callback = &mesh_end;

	options.output_vertex_normals = FALSE;// TRUE;
	options.output_face_app_data = TRUE;
	options.output_face_colours = FALSE;

	PFSolidGetMesh(solid, PV_MESH_TRIANGLES, &options);

	std::vector<float> floatArray;

	for (int i = 0; i < meshBuilder.faces.size(); i++)
	{
		int PtIndex = meshBuilder.faces[i];
		//PTPoint point = meshBuilder.points[PtIndex];
		floatArray.push_back(meshBuilder.points[PtIndex].x);
		floatArray.push_back(meshBuilder.points[PtIndex].y);
		floatArray.push_back(meshBuilder.points[PtIndex].z);
	}
	//solidToShellParams->shellKit.SetFacelist(shellBuilder.faces);
	//solidToShellParams->shellKit.SetPoints(shellBuilder.points);

	status = PFThreadUnregister();

	return floatArray;
}

struct connection_info_struct
{
	int connectiontype;
	char *file_name;
	FILE * pFile;
	struct MHD_PostProcessor *postprocessor;
}; 

#define POSTBUFFERSIZE  512
#define MAXNAMESIZE     20
#define MAXANSWERSIZE   512
#define GET             0
#define POST            1


static int
iterate_post(void *coninfo_cls,
	enum MHD_ValueKind kind,
	const char *key,
	const char *filename,
	const char *content_type,
	const char *transfer_encoding,
	const char *data,
	uint64_t off,
	size_t size)
{
	struct connection_info_struct *con_info = (connection_info_struct *) coninfo_cls;

	//if (0 == strcmp(key, "name"))
	//{
	//	if ((size > 0) && (size <= MAXNAMESIZE))
	//	{
	//		char *answerstring;
	//		answerstring = malloc(MAXANSWERSIZE);
	//		if (!answerstring) return MHD_NO;

	//		snprintf(answerstring, MAXANSWERSIZE, greatingpage, data);
	//		con_info->answerstring = answerstring;
	//	}
	//	else con_info->answerstring = NULL;

	//	return MHD_NO;
	//}

	return MHD_NO;// MHD_YES;
}

int answer_to_connection(void *cls, struct MHD_Connection *connection,
	const char *url,
	const char *method, const char *version,
	const char *upload_data,
	size_t *upload_data_size, void **con_cls)
{
	printf("--- New %s request for %s using version %s\n", method, url, version);
	//MHD_get_connection_values(connection, MHD_HEADER_KIND, &print_out_key, NULL);

	struct MHD_Response *response;
	int ret = 0;

	//int result = MHD_set_connection_value(connection,
	//	MHD_RESPONSE_HEADER_KIND, //MHD_HEADER_KIND,
	//	MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN,
	//	"*");

	if (strcmp(method, "POST") == 0 && strcmp(url, "/SetHollowAmount") == 0)
	{
		if (NULL == *con_cls) // new connection
		{
			struct connection_info_struct *con_info;

			con_info = (connection_info_struct *)malloc(sizeof(struct connection_info_struct));
			if (NULL == con_info) // failed to allocate memory for connection info
				return MHD_NO;

			con_info->connectiontype = POST;

			*con_cls = (void*)con_info;

			ret = MHD_YES;
		}
		else
		{
			fHollowAmount = atof(upload_data);

			response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);

			MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

			ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
			MHD_destroy_response(response);
		}
	}
	else if (strcmp(method, "POST") == 0 && strcmp(url, "/SetSimplifyAmount") == 0)
	{
		if (NULL == *con_cls) // new connection
		{
			struct connection_info_struct *con_info;

			con_info = (connection_info_struct *)malloc(sizeof(struct connection_info_struct));
			if (NULL == con_info) // failed to allocate memory for connection info
				return MHD_NO;

			con_info->connectiontype = POST;

			*con_cls = (void*)con_info;

			ret = MHD_YES;
		}
		else
		{
			nSimplifyAmount = atof(upload_data);

			response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);

			MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

			ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
			MHD_destroy_response(response);
		}
	}
	else if (strcmp(method, "POST") == 0)
	{
		//if (upload_data == NULL)
		if (NULL == *con_cls) // new connection
		{
			printf("File Upload Initiated\n");

			struct connection_info_struct *con_info;
			con_info = (connection_info_struct *)malloc(sizeof(struct connection_info_struct));
			if (NULL == con_info) // failed to allocate memory for connection info
				return MHD_NO;

			con_info->pFile = NULL;

			/*con_info->postprocessor = MHD_create_post_processor(connection, POSTBUFFERSIZE,	&iterate_post, (void*)con_info);

			if (NULL == con_info->postprocessor) // failfddfs
			{
				free(con_info);
				return MHD_NO;
			}*/
			con_info->connectiontype = POST;
			//con_info->pFile = fopen("temp.stl", "wb");
			char* file_name = (char*)&url[1];
			char path[1024];
			sprintf(path, "./data/%s", file_name);
			con_info->pFile = fopen(path, "wb");
			con_info->file_name = (char*)malloc(sizeof(char) * (strlen(path) +1));
			strcpy(con_info->file_name, path);
			*con_cls = (void*)con_info;
			
			ret = MHD_YES;
		}
		else
		{
			struct connection_info_struct *con_info = (connection_info_struct *)*con_cls;

			if (*upload_data_size != 0)
			{
				printf("Processing file chunk\n");

				size_t result = fwrite(upload_data, 1, *upload_data_size, con_info->pFile);
				
				/*MHD_post_process(con_info->postprocessor, upload_data,
					*upload_data_size);*/
				*upload_data_size = 0;

				return MHD_YES;
			}
			else
			{
				fclose(con_info->pFile);
				//return MHD_NO;
				PgVisitTesselation sPgVisitTesselation;
				//LoadFile(FILE_NAME, &sPgVisitTesselation);
				ret = LoadFile(con_info->file_name, &sPgVisitTesselation);

				if (ret == A3D_SUCCESS)
				{
					TesselationToSolid(&sPgVisitTesselation, &solid);

					std::vector<float> floatArray = SolidToFloatArray(solid);

					faceCount = floatArray.size() / 9;

					if (floatArray.size() > 0)
						response = MHD_create_response_from_buffer(floatArray.size() * sizeof(float), (void*)&floatArray[0], MHD_RESPMEM_MUST_COPY);
					else
						response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);
				}
				else // error loading
				{
					response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);
				}

				MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

				ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
				MHD_destroy_response(response);			
			}
		}
	}
	/*else if (strcmp(url, "/load") == 0)
	{
		PgVisitTesselation sPgVisitTesselation;
		LoadFile(FILE_NAME, &sPgVisitTesselation);

		TesselationToSolid(&sPgVisitTesselation, &solid);

		response = MHD_create_response_from_buffer(sPgVisitTesselation.GetPointsSize() * 3 * sizeof(float),
			(void*)sPgVisitTesselation.GetPoints(),
			MHD_RESPMEM_MUST_COPY);

		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}*/
	else if (strcmp(url, "/GetOpenEdges") == 0)
	{
		std::vector<float> floatArray = GetOpenEdges(&solid);

		if (floatArray.size() > 0)
			response = MHD_create_response_from_buffer(floatArray.size() * sizeof(float), (void*)&floatArray[0], MHD_RESPMEM_MUST_COPY);
		else
			response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);

		MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	else if (strcmp(url, "/GetIntersectingTriangles") == 0)
	{
		std::vector<float> floatArray = GetIntersectingTriangles(&solid);

		float *points = (float*)malloc(floatArray.size() * sizeof(float));
		for (int i = 0; i < floatArray.size(); i++) points[i] = floatArray[i];

		if (floatArray.size() > 0)
			response = MHD_create_response_from_buffer(floatArray.size() * sizeof(float), (void*)points/*&floatArray[0]*/, MHD_RESPMEM_MUST_COPY);
		else
			response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);

		MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	else if (strcmp(url, "/AutoHeal") == 0)
	{
		AutoHeal(solid);
		std::vector<float> floatArray = SolidToFloatArray(solid);

		faceCount = floatArray.size() / 9;

		if (floatArray.size() > 0)
			response = MHD_create_response_from_buffer(floatArray.size() * sizeof(float), (void*)&floatArray[0], MHD_RESPMEM_MUST_COPY);
		else
			response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);

		MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	} 
	else if (strcmp(url, "/Simplify") == 0)
	{
		Simplify(solid);
		std::vector<float> floatArray = SolidToFloatArray(solid);

		if (floatArray.size() > 0)
			response = MHD_create_response_from_buffer(floatArray.size() * sizeof(float), (void*)&floatArray[0], MHD_RESPMEM_MUST_COPY);
		else
			response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);

		MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	else if (strcmp(url, "/Hollow") == 0)
	{
		Hollow(solid);
		std::vector<float> floatArray = SolidToFloatArray(solid);

		if (floatArray.size() > 0)
			response = MHD_create_response_from_buffer(floatArray.size() * sizeof(float), (void*)&floatArray[0], MHD_RESPMEM_MUST_COPY);
		else
			response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);

		MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	else if (strcmp(url, "/GetModelStatus") == 0)
	{
		PTStatus status = PFThreadRegister();

		int props[4];

		props[0] = (int)PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_CLOSED);
		props[1] = (int)PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_MANIFOLD);
		props[2] = !(int)PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_SELF_INTERSECTS);
		props[3] = !(int)PFEntityGetBooleanProperty(solid, PV_SOLID_PROP_BAD_ORIENTATION);

		status = PFThreadUnregister();

		response = MHD_create_response_from_buffer(4 * sizeof(int), (void*)&props[0], MHD_RESPMEM_MUST_COPY);

		MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	else if (strcmp(url, "/GetModelStatistics") == 0)
	{
		PTStatus status = PFThreadRegister();

		float statistics[6];

		statistics[0] = PFEntityGetNat32Property(solid, PV_SOLID_PROP_NUM_VERTICES); // points
		statistics[1] = PFEntityGetNat32Property(solid, PV_SOLID_PROP_NUM_FACES); // faces

		PTBounds bounds;
		PFEntityGetBoundsProperty(solid, PV_SOLID_PROP_BOUNDS, bounds);

		statistics[2] = bounds[1] - bounds[0]; // xdim
		statistics[3] = bounds[3] - bounds[2]; // ydim
		statistics[4] = bounds[5] - bounds[4]; // zdim

		statistics[5] = (float)PFEntityGetDoubleProperty(solid, PV_SOLID_PROP_VOLUME); // volume

		response = MHD_create_response_from_buffer(6 * sizeof(float), (void*)&statistics[0], MHD_RESPMEM_MUST_COPY);

		MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);

		status = PFThreadUnregister();

	}
	else if (strcmp(url, "/CloseSession") == 0)
	{

		PTStatus status = PFThreadRegister();

		if (solid != NULL)
		{
			PFSolidDestroy(solid);
			solid = NULL;
		}

		status = PFThreadUnregister();

		if (strlen(filename) > 0)
		{
			remove(filename);
		}

		response = MHD_create_response_from_buffer(0, (void*)NULL, MHD_RESPMEM_MUST_COPY);

		MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");

		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	return ret;
}

void request_completed(void *cls, struct MHD_Connection *connection,
	void **con_cls,
	enum MHD_RequestTerminationCode toe)
{
	struct connection_info_struct *con_info = (connection_info_struct *)*con_cls;

	if (NULL == con_info) return;
	if (con_info->connectiontype == POST)
	{
		//MHD_destroy_post_processor(con_info->postprocessor);
		//if (con_info->answerstring) free(con_info->answerstring);
		//if (con_info->answerstring) free(con_info->answerstring);
	}

	free(con_info);
	*con_cls = NULL;
}

int InitializeDataDir()
{
	DWORD attribs = ::GetFileAttributesA("./data");
	if (attribs == INVALID_FILE_ATTRIBUTES) {
		printf("Data directory doesn't exist\n");
		_mkdir("data");
	}
//		return (attribs & FILE_ATTRIBUTE_DIRECTORY);

	return 0;
}

int main()
{
	InitializeDataDir();

	if (InitializeExchange())
		return -1;

	if (!InitializePolygonica())
	{
		struct MHD_Daemon *daemon;
		daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
			&answer_to_connection, NULL, 
			MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL,
			MHD_OPTION_END);

		if (NULL == daemon) return 1;

		getchar();

		MHD_stop_daemon(daemon);

		// Terminate Polygonica
		PTStatus status = PFTerminate();
		if (status) printf("Polygonica failed to terminate.\n");
	}

	// Terminate Exchange
	A3DDllTerminate();
	A3DSDKUnloadLibrary();

	return 0;
}

