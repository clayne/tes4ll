#include "../include/llexportmeshtonif.h"

#include "../../lltool/include/llmaplist.h"

#include "../../niflib/include/obj/NiTexturingProperty.h"
#include "../../niflib/include/obj/NiSourceTexture.h"
#include "../../niflib/include/obj/bsshadertextureset.h"
#include "../../niflib/include/obj/BSShaderPPLightingProperty.h"
#include "../../niflib/include/obj/BSSegmentedTriShape.h"
#include "../../niflib/include/obj/NiAdditionalGeometryData.h"

Niflib::NiNode *llExportMeshToNif::ninode_ptr = NULL;

//constructor
llExportMeshToNif::llExportMeshToNif() : llExportMeshToObj() {
	SetCommandName("ExportMeshToNif");
}

int llExportMeshToNif::Prepare(void) {
	if (!llExportMeshToObj::Prepare()) return 0;

	useshapes  = 0;
	makeninode = 0;
	segmented  = 0;
	addgeometrydata = 0;
	no_uv = 0;

	texset1 = NULL;
	texset2 = NULL;

	loc_trans_x = loc_trans_y = loc_trans_z = 0;

	return 1;
}

int llExportMeshToNif::RegisterOptions(void) {
	if (!llExportMeshToObj::RegisterOptions()) return 0;

	RegisterFlag ("-useshapes",  &useshapes);
	RegisterFlag ("-makeninode", &makeninode);
	RegisterFlag ("-addgeometrydata", &addgeometrydata);
	RegisterFlag ("-no_uv", &no_uv);
	
	RegisterValue("-texset1", &texset1);
	RegisterValue("-texset2", &texset2);

	RegisterValue("-segments",  &segmented);

	RegisterValue("-loc_transx", &loc_trans_x);
	RegisterValue("-loc_transy", &loc_trans_y);
	RegisterValue("-loc_transz", &loc_trans_z);

	return 1;
}


int llExportMeshToNif::Exec(void) {
	if (!llTriMod::Exec()) return 0;

	if (!Used("-filename"))
	    //filename = (char *)"map.nif";
		filename = NULL;
	if (!MakeSelection()) return 0;

	//look for _install_dir:
	if (_llUtils()->GetValue("_install_dir") && filename) {
		char *filename_tmp = new char[strlen(filename) + strlen(_llUtils()->GetValue("_install_dir")) + 2];
		sprintf_s(filename_tmp, strlen(filename) + strlen(_llUtils()->GetValue("_install_dir")) + 2, "%s\\%s", 
			_llUtils()->GetValue("_install_dir"), filename);
		//std::cout << _llUtils()->GetValue("_install_dir") << ":" << filename << std::endl;
		filename = filename_tmp;
	}

	float cellsize_x = 0;
	if (_llUtils()->GetValueF("_cellsize_x"))
		cellsize_x = (float)(*_llUtils()->GetValueF("_cellsize_x"));
	float cellsize_y = 0;
	if (_llUtils()->GetValueF("_cellsize_y"))
		cellsize_y = (float)(*_llUtils()->GetValueF("_cellsize_y"));

	//Now the nif-specific part:

	using namespace Niflib;

	if (makeninode) {
		ninode_ptr = new NiNode;
	}

	int num_triangles = newtriangles->GetN();
    std::vector<Triangle> t(num_triangles);

	NiAVObjectRef     myavobj;
	NiGeometryDataRef mygeomobj;

	if (useshapes) {

		NiTriShape *node_ptr;
		vector<BSSegment > segments;
		segments.resize(segmented);

		if (segmented) {
			node_ptr = new BSSegmentedTriShape;	
		} else {
			node_ptr = new NiTriShape;
		}

		NiTriShapeRef  node     = node_ptr;
		myavobj                 = node_ptr;

		NiTriShapeData   *node2_ptr = new NiTriShapeData();
		NiTriShapeDataRef node2     = node2_ptr;
		mygeomobj                   = node2_ptr;
		
		node_ptr->SetData(node2);
		node_ptr->SetFlags(14);
		node_ptr->SetLocalTranslation(Vector3 (loc_trans_x, loc_trans_y, loc_trans_z));

		node2_ptr->SetVertices(reinterpret_cast<std::vector<Niflib::Vector3> & >(newpoints->GetVertices()));   
		node2_ptr->SetTspaceFlag(16);

		int running_segment = 0, last_tri = 0, cell_x, cell_y;
		for (int i=0; i<num_triangles; i++) {
			if (segmented) {
				int new_cell_x = (int)floor(newtriangles->GetTriangleCenterX(i)/cellsize_x);
				int new_cell_y = (int)floor(newtriangles->GetTriangleCenterY(i)/cellsize_y);
				if (i==0) {
					cell_x = new_cell_x;
					cell_y = new_cell_y;
				} else {
					segments[running_segment].offset = 3*last_tri;
					segments[running_segment].count = i - last_tri + 1;
					if (cell_x != new_cell_x || cell_y != new_cell_y) {
						cell_x = new_cell_x;
						cell_y = new_cell_y;
						if (running_segment < segmented) {
							segments[running_segment].count = i - last_tri;
							last_tri = i;
							running_segment++;	
							//std::cout << segments[running_segment].offset << ":" <<
							//segments[running_segment].count << std::endl;
						}
					}
				}
			}
			t[i].v1 = newtriangles->GetPoint1(i);
			t[i].v2 = newtriangles->GetPoint2(i);
			t[i].v3 = newtriangles->GetPoint3(i);
		}

		if (segmented) {
			((BSSegmentedTriShape*)node_ptr)->numSegments = segmented;
			((BSSegmentedTriShape*)node_ptr)->segment = segments;
		} 

		node2_ptr->SetTriangles(t);
		if (no_uv) {
			node2_ptr->SetUVSetCount(0);
		} else {
			node2_ptr->SetUVSetCount(1);
			node2_ptr->SetUVSet(0, reinterpret_cast<std::vector<Niflib::TexCoord> & >(newpoints->GetUV()));
		}

		//int stripcount = node2_ptr->GetStripCount();

		vector<Triangle> newt=node2_ptr->GetTriangles();

		_llLogger()->WriteNextLine(-LOG_INFO, "The (shape-based) mesh %s has %i triangles and %i vertices",
			filename, newt.size(), newpoints->GetVertices().size());

		//Vector3 center = node2_ptr->GetCenter();
		//center.z -= 7000;
		//node2_ptr->SetCenter(center);
		//node2_ptr->SetRadius(15000);

		if (ninode_ptr) 
			ninode_ptr->AddChild(node_ptr);

	} else {

		NiTriStrips   *node_ptr = new NiTriStrips;
		NiTriStripsRef node     = node_ptr;
		myavobj                 = node_ptr;

		NiTriStripsData   *node2_ptr = new NiTriStripsData();
		NiTriStripsDataRef node2     = node2_ptr;
		mygeomobj                    = node2_ptr;

		node_ptr->SetData(node2);
		node_ptr->SetFlags(14);
		node_ptr->SetLocalTranslation(Vector3 (loc_trans_x, loc_trans_y, loc_trans_z));

		node2_ptr->SetVertices(reinterpret_cast<std::vector<Niflib::Vector3> & >(newpoints->GetVertices()));   
		node2_ptr->SetTspaceFlag(16);

		newtriangles->Stripification();
		node2_ptr->SetStripCount(1);
		node2_ptr->SetStrip(0, newtriangles->GetVertices());

		node2_ptr->SetTriangles(t);
		if (no_uv) {
			node2_ptr->SetUVSetCount(0);
		} else {
			node2_ptr->SetUVSetCount(1);
			node2_ptr->SetUVSet(0, reinterpret_cast<std::vector<Niflib::TexCoord> & >(newpoints->GetUV()));
		}

		int stripcount = node2_ptr->GetStripCount();

		vector<Triangle> newt=node2_ptr->GetTriangles();

		_llLogger()->WriteNextLine(-LOG_INFO, "The (stripe-based) mesh %s has %i triangles and %i vertices",
			filename, newt.size(), newpoints->GetVertices().size());

		if (ninode_ptr) 
			ninode_ptr->AddChild(node_ptr);
	}
	
	if (texname) {
		//optional single texture
		NiTexturingProperty * texture_ptr = new NiTexturingProperty();
		NiTexturingPropertyRef texture = texture_ptr;
		NiSourceTexture * image_ptr = new NiSourceTexture();
		NiSourceTextureRef image = image_ptr;
		image->SetExternalTexture(texname);
		TexDesc tex;
		tex.source = image_ptr;
		texture_ptr->SetTexture(0, tex);
		myavobj->AddProperty(texture);
	}

	if (texset1 || texset2) {
		BSShaderPPLightingProperty * shader_ptr = new BSShaderPPLightingProperty();
		BSShaderPPLightingPropertyRef shader = shader_ptr;
		BSShaderTextureSet * txst_ptr = new BSShaderTextureSet();
		BSShaderTextureSetRef txst = txst_ptr;
		shader->SetTextureSet(txst);
		shader->SetShaderFlags(BSShaderFlags(0x3000));
		shader->SetUnknownInt2(2);
		shader->SetUnknownInt3(0);
		shader->SetUnknownFloat2(0.f);
		shader->SetUnknownFloat4(8.f);
		shader->SetUnknownFloat5(1.f);
		shader->SetEnvmapScale(1.0);

		txst->SetTexture(0, texset1); // color
		txst->SetTexture(1, texset2); // normal		
		for (int i=2; i<6; i++) txst->SetTexture(i, "");

		myavobj->AddProperty(shader);
	}

	if (addgeometrydata) {

		NiAdditionalGeometryDataRef geom = new NiAdditionalGeometryData();
		geom->SetNumVertices(newpoints->GetN());
		
		std::vector<AdditionalDataInfo> info;
		info.resize(1);
		info[0].dataType = 1;
		info[0].numChannelBytesPerElement = 4;
		info[0].numTotalBytesPerElement = 4;
		info[0].numChannelBytes = 4 * newpoints->GetN();
		info[0].unknownByte1 = 2;

		std::vector<AdditionalDataBlock> block;
		block.resize(1);
		block[0].hasData = true;
		block[0].blockSize = 4 * newpoints->GetN();
		block[0].numBlocks = 1;
		
		std::vector<int> data_size;
		data_size.resize(1);
		data_size[0] = 4;
		block[0].dataSizes = data_size;
		
		vector<int> offsets;
		offsets.resize(1);
		offsets[0] = 0;
		block[0].blockOffsets = offsets;

		block[0].numData = 1;

		std::vector<byte> data1;
		data1.resize(4 * newpoints->GetN());
		//fill data structure
		for (int i=0; i<newpoints->GetN(); i++) {
			byte value[4];
			*(float*)value = newpoints->GetZ(i);
			data1[i*4] = value[0];
			data1[i*4+1] = value[1];
			data1[i*4+2] = value[2];
			data1[i*4+3] = value[3];
		}

		std::vector< vector<byte> > data2;
		data2.resize(1);
		data2[0] = data1;
		block[0].data = data2;

		geom->SetDataInfo(info);
		geom->SetDataBlock(block);

		mygeomobj->SetAdditionalGeometryData((AbstractAdditionalGeometryDataRef) geom);

	}

	if (ninode_ptr) {
		if (filename) {
			NifInfo info = NifInfo();
			info.version = 335544325;
			if (_llUtils()->GetValue("_nif_version"))
				sscanf(_llUtils()->GetValue("_nif_version"), "%u", &(info.version));
			WriteNifTree(filename, ninode_ptr, info);
			ninode_ptr = NULL;
		}
	} else if (filename) {
		NifInfo info = NifInfo();
		info.version = 335544325;
		if (_llUtils()->GetValue("_nif_version"))
			sscanf(_llUtils()->GetValue("_nif_version"), "%u", &(info.version));
		WriteNifTree(filename, myavobj, info);
	} else {
		_llLogger()->WriteNextLine(-LOG_WARNING, "No filename, no NiNode...");
	}

	if (_llUtils()->GetValue("_install_dir") && filename) {
		delete filename;
	}

	return 1;
}
