/*******************************************************************************
*
* Copyright (c) 2011 by Tech Soft 3D, LLC.
* The information contained herein is confidential and proprietary to
* Tech Soft 3D, LLC., and considered a trade secret as defined under
* civil and criminal statutes.  Tech Soft 3D shall pursue its civil
* and criminal remedies in the event of unauthorized use or misappropriation
* of its trade secrets.  Use of this information by anyone other than
* authorized employees of Tech Soft 3D, LLC. is granted only under a
* written non-disclosure agreement, expressly prescribing the scope and
* manner of such use.
*
*******************************************************************************/
   
#include "TessConnector.h"
#include "Visitors.h"
#include <algorithm>
////////////////////////////////////////
//	A3DTessDataConnector
///////////////////////////////////////


A3DTessDataConnector::A3DTessDataConnector(const A3DTess3D* pTess3D) : A3DConnector(pTess3D) 
{
	A3D_INITIALIZE_DATA(A3DTess3DData, m_sTessData);
	A3DTess3DGet( pTess3D, &m_sTessData);
	
	A3D_INITIALIZE_DATA(A3DTessBaseData, m_sBaseTessData);
	A3DTessBaseGet((const A3DTessBase*) pTess3D,&m_sBaseTessData);
}

A3DTessDataConnector::~A3DTessDataConnector()
{
	 A3DTess3DGet( NULL, &m_sTessData);
	 A3DTessBaseGet(NULL, &m_sBaseTessData);
}


A3DStatus A3DTessDataConnector::Points(double*& pPoint, unsigned int& uSize) const
{
	pPoint = m_sBaseTessData.m_pdCoords;
	uSize = m_sBaseTessData.m_uiCoordSize;
	return A3D_SUCCESS;
}
	
A3DStatus A3DTessDataConnector::Normals(double*& pNormal, unsigned int& uSize)	const
{
	pNormal = m_sTessData.m_pdNormals;
	uSize = m_sTessData.m_uiNormalSize;
	return A3D_SUCCESS;
}



#define COPY(dest,src,size_)	if(size_ != 0)	{\
		unsigned uTempSize = dest.size();\
		dest.resize(uTempSize + size_);\
		std::copy(src ,src+ size_, dest.begin() + uTempSize);\
		src+= size_; \
		size_ = 0;\
	}
	  
#define COPY_(dest,src,size_) if(size_) {\
		unsigned uTempSize = dest.size();\
		dest.resize(uTempSize + size_);\
		std::copy(src,src+size_, dest.begin() + uTempSize);\
	}
		
	
A3DStatus A3DTessDataConnector::IndicesAsTriangleWithNormals(
					std::vector<unsigned>& auIndices) const
{	 
	A3DStatus iRet = A3D_SUCCESS;
	unsigned uFaceCount, uFaceSize = m_sTessData.m_uiFaceTessSize;
	for(uFaceCount = 0;uFaceCount < uFaceSize;	 uFaceCount++)
	{
		CHECK_RET(IndicesPerFaceAsTriangleWithNormals(
					uFaceCount,
					auIndices));
	}
	return A3D_SUCCESS;
}

A3DStatus A3DTessDataConnector::IndicesPerFaceAsTriangleWithNormals(
					const unsigned& uFaceIndice,
					std::vector<unsigned>& auIndices) const
{
	A3DTessFaceData* pFaceTessData =	&(m_sTessData.m_psFaceTessData[uFaceIndice]);
	if (!pFaceTessData->m_uiSizesTriangulatedSize)
		return A3D_SUCCESS;

	A3DUns32* puiTriangulatedIndexes = m_sTessData.m_puiTriangulatedIndexes 
									+ pFaceTessData->m_uiStartTriangulated;
	unsigned uiCurrentSize = 0;
    if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangle)
	{
		unsigned uiNbIndice = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] * 6;
		unsigned int itoto = auIndices.size();
		COPY(auIndices, puiTriangulatedIndexes, uiNbIndice);
			 

		
	}

	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleFan)
	{
		A3DUns32 uiNbFan = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		for (A3DUns32 uiFan = 0; uiFan < uiNbFan; uiFan++)
		{	
			A3DUns32 uiNbPoint = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
			A3DUns32* pFanPointIndice = puiTriangulatedIndexes;
			puiTriangulatedIndexes+=2;
			A3DUns32 uIPoint;
			for (uIPoint = 0; uIPoint < uiNbPoint - 2; uIPoint);
			{	
				COPY_(auIndices, pFanPointIndice, 2);
				COPY_(auIndices, puiTriangulatedIndexes, 2);
				COPY_(auIndices, puiTriangulatedIndexes, 2);
				puiTriangulatedIndexes+=2;
			}
		}
	}
	
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleStripe)
	{
		A3DUns32 uiNbStripe = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		A3DUns32 uiStripe;
		for (uiStripe = 0; uiStripe < uiNbStripe; uiStripe++)
		{	A3DUns32 uiNbPoint = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
			puiTriangulatedIndexes+=2;
			A3DUns32 uIPoint;
			for (uIPoint = 0; uIPoint < uiNbPoint - 2; uIPoint);
			{
				COPY_(auIndices, puiTriangulatedIndexes,2);
				if(	uIPoint%2)
				{
					COPY_(auIndices, puiTriangulatedIndexes - 2,2);
					COPY_(auIndices, puiTriangulatedIndexes + 2,2);
					 
				}
				else
				{
					COPY_(auIndices, puiTriangulatedIndexes + 2,2);
					COPY_(auIndices, puiTriangulatedIndexes - 2,2);
				}
				puiTriangulatedIndexes+=2;
			}
		}
	}
	
	//One Normal Per entity
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleOneNormal)
	{
		A3DUns32 uNbTriangles = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		
		for(A3DUns32 uI = 0; uI < uNbTriangles; uI++)
		{
		    A3DUns32* pNormalIndice = puiTriangulatedIndexes;
			COPY_(auIndices, puiTriangulatedIndexes,2);
			puiTriangulatedIndexes+=2;
			COPY_(auIndices, pNormalIndice,1);
			COPY_(auIndices, puiTriangulatedIndexes,1);
			puiTriangulatedIndexes+=1;
			COPY_(auIndices, pNormalIndice,1);
			COPY_(auIndices, puiTriangulatedIndexes,1);
			puiTriangulatedIndexes+=1;
		}
	}
    
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleFanOneNormal)
	{
		A3DUns32 uiNbFan = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		for (A3DUns32 uiFan = 0; uiFan < uiNbFan; uiFan++)
		{	
			A3DUns32 uiNbPoint = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
			A3DUns32* pFanPointIndice = puiTriangulatedIndexes;
			puiTriangulatedIndexes+=1;
			for (A3DUns32 uIPoint = 0; uIPoint < uiNbPoint - 2; uIPoint);
			{	
				COPY_(auIndices, pFanPointIndice, 1);
				COPY_(auIndices, pFanPointIndice + 1, 1);
				COPY_(auIndices, pFanPointIndice, 1);
				COPY_(auIndices, puiTriangulatedIndexes, 1);
				COPY_(auIndices, pFanPointIndice, 1);
				COPY_(auIndices, puiTriangulatedIndexes + 1, 1);
				puiTriangulatedIndexes+=1;
			}
		}
	}
	
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleStripeOneNormal)
	{
		A3DUns32 uiNbStripe = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		A3DUns32 uiStripe;
		for (uiStripe = 0; uiStripe < uiNbStripe; uiStripe++)
		{	
			A3DUns32* pStripeNormalIndice = puiTriangulatedIndexes;
			A3DUns32 uiNbPoint = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
			puiTriangulatedIndexes+=1;
			A3DUns32 uIPoint;
			for (uIPoint = 0; uIPoint < uiNbPoint - 2; uIPoint);
			{
				COPY_(auIndices, pStripeNormalIndice,1)
				COPY_(auIndices, puiTriangulatedIndexes,1);
				if(	uIPoint%2)
				{
					COPY_(auIndices, pStripeNormalIndice,1)
					COPY_(auIndices, puiTriangulatedIndexes - 1, 1);
					COPY_(auIndices, pStripeNormalIndice,1)
					COPY_(auIndices, puiTriangulatedIndexes + 1, 1);
				}
				else
				{
					COPY_(auIndices, puiTriangulatedIndexes + 1, 1);
					puiTriangulatedIndexes+=1;
					COPY_(auIndices, pStripeNormalIndice,1)
					COPY_(auIndices, puiTriangulatedIndexes - 1, 1);
					puiTriangulatedIndexes+=1;
				}
				puiTriangulatedIndexes+=1;
			}
		}
	}
	
	// Textured
	//One Normal Per entity
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleTextured
	|| pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleFanTextured
	|| pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleStripeTextured
	|| pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleOneNormalTextured
	|| pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleFanOneNormalTextured
	|| pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleStripeOneNormalTextured)
		return A3D_ERROR; // Textured not managed by this fuctions

	return	 A3D_SUCCESS;
}

A3DStatus A3DTessDataConnector::IndicesPerFace(
					const unsigned& uFaceIndice,
					std::vector<unsigned>& auTrianglesOneNormal,			//(normal, point, point,..., point)
					std::vector<unsigned>& auFanOneNormal,
					std::vector<unsigned>& auStripeOneNormal,
					std::vector<unsigned>& auTrianglesWithNormals,			//normal, point, ... , normal, point
					std::vector<unsigned>& auFanWithNormals,	   
					std::vector<unsigned>& auStripeWithNormal,	
					std::vector<unsigned>& auTrianglesOneNormalTexture,		//normal,{texture...},point,{texture...},point,{texture...},point
					std::vector<unsigned>& auFanOneNormalTextured,		   
					std::vector<unsigned>& auStripeOneNormalTextured,
					std::vector<unsigned>& auTrianglesWithNormalsTextured,	//normal,{texture...},point,...,normal,{texture...},point
					std::vector<unsigned>& auFanWithNormalsTextured,
					std::vector<unsigned>& auStripeWithNormalsTextured) const
{
	A3DTessFaceData* pFaceTessData =	&(m_sTessData.m_psFaceTessData[uFaceIndice]);
	if (!pFaceTessData->m_uiSizesTriangulatedSize)
		return A3D_SUCCESS;

	A3DUns32 uiCurrentSize = 0;
	A3DUns32* puiTriangulatedIndexes = m_sTessData.m_puiTriangulatedIndexes 
									+ pFaceTessData->m_uiStartTriangulated;
    A3DUns32 uiNbIndice = 0;
	
	// One Normal Per Vertex
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangle)
	{
		uiNbIndice = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] * 6;
		COPY(auTrianglesWithNormals, puiTriangulatedIndexes, uiNbIndice);
	}

	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleFan)
	{
		A3DUns32 uiNbFan = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		for (A3DUns32 uiFan = 0; uiFan < uiNbFan; uiFan++)
			uiNbIndice+= pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++]*2;
	 	COPY(auFanWithNormals, puiTriangulatedIndexes, uiNbIndice);
	}
	
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleStripe)
	{
		A3DUns32 uiNbStripe = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		A3DUns32 uiStripe;
		for (uiStripe = 0; uiStripe < uiNbStripe; uiStripe++)
			uiNbIndice+= pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++]*2;
		COPY(auStripeWithNormal, puiTriangulatedIndexes, uiNbIndice);
	}
	
	//One Normal Per entity
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleOneNormal)
		uiNbIndice = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] * 4;
	COPY(auTrianglesWithNormals, puiTriangulatedIndexes, uiNbIndice);
    
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleFanOneNormal)
	{
		A3DUns32 uiNbFan = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		for (A3DUns32 uiFan = 0; uiFan < uiNbFan; uiFan++)
			uiNbIndice+= pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] + 1;
	 	COPY(auFanWithNormals, puiTriangulatedIndexes, uiNbIndice);
	}
	
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleStripeOneNormal)
	{
		A3DUns32 uiNbStripe = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		A3DUns32 uiStripe;
		for (uiStripe = 0; uiStripe < uiNbStripe; uiStripe++)
			uiNbIndice+= pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] + 1;
		COPY(auStripeWithNormal, puiTriangulatedIndexes, uiNbIndice);
	}
	
	// Textured
	//One Normal Per entity
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleTextured)
		uiNbIndice = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] * 9;
	COPY(auTrianglesWithNormals, puiTriangulatedIndexes, uiNbIndice);
    
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleFanTextured)
	{
		A3DUns32 uiNbFan = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		for (A3DUns32 uiFan = 0; uiFan < uiNbFan; uiFan++)
			uiNbIndice+= pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] * 3 ;
	 	COPY(auFanWithNormals, puiTriangulatedIndexes, uiNbIndice);
	}
	
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleStripeTextured)
	{
		A3DUns32 uiNbStripe = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		A3DUns32 uiStripe;
		for (uiStripe = 0; uiStripe < uiNbStripe; uiStripe++)
			uiNbIndice+= pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] * 3;
		COPY(auStripeWithNormal, puiTriangulatedIndexes, uiNbIndice);
	}


	// Textured
	//One Normal Per entity
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleOneNormalTextured)
		uiNbIndice = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] * 7;
	COPY(auTrianglesWithNormals, puiTriangulatedIndexes, uiNbIndice);
    
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleFanOneNormalTextured)
	{
		A3DUns32 uiNbFan = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		for (A3DUns32 uiFan = 0; uiFan < uiNbFan; uiFan++)
			uiNbIndice+= pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] * 2 + 1 ;
	 	COPY(auFanWithNormals, puiTriangulatedIndexes, uiNbIndice);
	}

	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleStripeOneNormalTextured)
	{
		A3DUns32 uiNbStripe = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];
		A3DUns32 uiStripe;
		for (uiStripe = 0; uiStripe < uiNbStripe; uiStripe++)
			uiNbIndice+= pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++] * 2 + 1;
		COPY(auStripeWithNormal, puiTriangulatedIndexes, uiNbIndice);
	}

	return	 A3D_SUCCESS;
}

A3DStatus A3DTessDataConnector::Indices(	
					std::vector<unsigned>& auTrianglesOneNormal,			//(normal, point, point,..., point)
					std::vector<unsigned>& auFanOneNormal,
					std::vector<unsigned>& auStripeOneNormal,
					std::vector<unsigned>& auTrianglesWithNormals,		//normal, point, ... , normal, point
					std::vector<unsigned>& auFanWithNormals,	   
					std::vector<unsigned>& auStripeWithNormal,	
					std::vector<unsigned>& auTrianglesOneNormalTexture,	//normal,{texture...},point,{texture...},point,{texture...},point
					std::vector<unsigned>& auFanOneNormalTextured,		   
					std::vector<unsigned>& auStripeOneNormalTextured,
					std::vector<unsigned>& auTrianglesWithNormalsTextured, //normal,{texture...},point,...,normal,{texture...},point
					std::vector<unsigned>& auFanWithNormalsTextured,
					std::vector<unsigned>& auStripeWithNormalsTextured)	const
{
	A3DStatus iRet = A3D_SUCCESS;
	unsigned uFaceCount, uFaceSize = m_sTessData.m_uiFaceTessSize;
	for(uFaceCount = 0;uFaceCount < uFaceSize;	 uFaceCount++)
		CHECK_RET(IndicesPerFace(
					uFaceCount,
					auTrianglesOneNormal,		
					auFanOneNormal,
					auStripeOneNormal,
					auTrianglesWithNormals,		
					auFanWithNormals,	   
					auStripeWithNormal,	
					auTrianglesOneNormalTexture,		
					auFanOneNormalTextured,		   
					auStripeOneNormalTextured,
					auTrianglesWithNormalsTextured,	
					auFanWithNormalsTextured,
					auStripeWithNormalsTextured));
	return A3D_SUCCESS;
}