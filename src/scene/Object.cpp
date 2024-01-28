/*
 * Copyright 2011-2022 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Based on:
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code'). 

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General Public 
License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see 
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of these 
additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Arx 
Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane Studios, c/o 
ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
// Code: Cyril Meynier
//
// Copyright (c) 1999 ARKANE Studios SA. All rights reserved

#include "scene/Object.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "core/Config.h"
#include "core/Core.h"

#include "game/Entity.h"
#include "game/EntityManager.h"

#include "graphics/GraphicsTypes.h"
#include "graphics/Math.h"
#include "graphics/data/FTL.h"
#include "graphics/data/TextureContainer.h"

#include "io/fs/FilePath.h"
#include "io/fs/SystemPaths.h"
#include "io/resource/ResourcePath.h"
#include "io/resource/PakReader.h"
#include "io/log/Logger.h"

#include "physics/CollisionShapes.h"
#include "physics/Physics.h"

#include "scene/LinkedObject.h"
#include "scene/GameSound.h"
#include "scene/Interactive.h"
#include "scene/Light.h"

#include "util/HandleContainer.h"
#include "util/String.h"


VertexId getNamedVertex(const EERIE_3DOBJ * eobj, std::string_view text) {
	
	if(!eobj) {
		return { };
	}
	
	for(const EERIE_ACTIONLIST & action : eobj->actionlist) {
		if(action.name == text) {
			return action.idx;
		}
	}
	
	return { };
}

VertexGroupId getGroupForVertex(const EERIE_3DOBJ * eobj, VertexId vertex) {
	
	if(!eobj) {
		return { };
	}
	
	for(VertexGroupId group : eobj->grouplist.handles() | boost::adaptors::reversed) {
		for(VertexId index : eobj->grouplist[group].indexes) {
			if(index == vertex) {
				return group;
			}
		}
	}
	
	return { };
}

void EERIE_Object_Precompute_Fast_Access(EERIE_3DOBJ * object) {
	
	if(!object) {
		return;
	}
	
	object->fastaccess.view_attach       = getNamedVertex(object, "view_attach");
	object->fastaccess.primary_attach    = getNamedVertex(object, "primary_attach");
	object->fastaccess.left_attach       = getNamedVertex(object, "left_attach");
	object->fastaccess.weapon_attach     = getNamedVertex(object, "weapon_attach");
	object->fastaccess.fire              = getNamedVertex(object, "fire");
	
	object->fastaccess.head_group = EERIE_OBJECT_GetGroup(object, "head");
	if(object->fastaccess.head_group) {
		object->fastaccess.head_group_origin = object->grouplist[object->fastaccess.head_group].origin;
	}
	
	object->fastaccess.sel_head     = EERIE_OBJECT_GetSelection(object, "head");
	object->fastaccess.sel_chest    = EERIE_OBJECT_GetSelection(object, "chest");
	object->fastaccess.sel_leggings = EERIE_OBJECT_GetSelection(object, "leggings");
}

void MakeUserFlag(TextureContainer * tc) {
	
	if(!tc)
		return;
	
	std::string_view tex = tc->m_texName.string();
	
	if(boost::contains(tex, "npc_")) {
		tc->userflags |= POLY_LATE_MIP;
	}
	
	if(boost::contains(tex, "nocol")) {
		tc->userflags |= POLY_NOCOL;
	}
	
	if(boost::contains(tex, "climb")) {
		tc->userflags |= POLY_CLIMB;
	}
	
	if(boost::contains(tex, "fall")) {
		tc->userflags |= POLY_FALL;
	}
	
	if(boost::contains(tex, "lava")) {
		tc->userflags |= POLY_LAVA;
	}
	
	if(boost::contains(tex, "water") || boost::contains(tex, "spider_web")) {
		tc->userflags |= POLY_WATER;
		tc->userflags |= POLY_TRANS;
	} else if(boost::contains(tex, "[metal]")) {
		tc->userflags |= POLY_METAL;
	}
	
}

EERIE_3DOBJ * Eerie_Copy(const EERIE_3DOBJ * obj) {
	
	EERIE_3DOBJ * nouvo = new EERIE_3DOBJ();
	
	nouvo->vertexlist = obj->vertexlist;
	nouvo->vertexWorldPositions.resize(nouvo->vertexlist.size());
	nouvo->vertexClipPositions.resize(nouvo->vertexlist.size());
	nouvo->vertexColors.resize(nouvo->vertexlist.size());
	
	nouvo->file = obj->file;
	
	nouvo->origin = obj->origin;
	
	nouvo->facelist = obj->facelist;
	nouvo->grouplist = obj->grouplist;
	nouvo->actionlist = obj->actionlist;
	nouvo->selections = obj->selections;
	nouvo->materials = obj->materials;
	nouvo->fastaccess = obj->fastaccess;
	
	EERIE_CreateCedricData(nouvo);
	
	if(obj->pbox) {
		nouvo->pbox = std::make_unique<PHYSICS_BOX_DATA>();
		nouvo->pbox->stopcount = 0;
		nouvo->pbox->radius = obj->pbox->radius;
		nouvo->pbox->vert = obj->pbox->vert;
	}
	
	nouvo->linked.clear();
	nouvo->originalMaterials.clear();
	
	return nouvo;
}

VertexSelectionId EERIE_OBJECT_GetSelection(const EERIE_3DOBJ * obj, std::string_view selname) {
	
	if(!obj) {
		return { };
	}
	
	for(VertexSelectionId selection : obj->selections.handles()) {
		if(obj->selections[selection].name == selname) {
			return selection;
		}
	}
	
	return { };
}

VertexGroupId EERIE_OBJECT_GetGroup(const EERIE_3DOBJ * obj, std::string_view groupname) {
	
	if(!obj) {
		return { };
	}
	
	for(VertexGroupId group : obj->grouplist.handles()) {
		if(obj->grouplist[group].name == groupname) {
			return group;
		}
	}
	
	return { };
}

static VertexGroupId getParentGroup(EERIE_3DOBJ * eobj, VertexGroupId child) {
	
	for(VertexGroupId group : eobj->grouplist.handles(0, size_t(child)) | boost::adaptors::reversed) {
		for(VertexId index : eobj->grouplist[group].indexes) {
			if(index == eobj->grouplist[child].origin) {
				return group;
			}
		}
	}
	
	return { };
}

void EERIE_CreateCedricData(EERIE_3DOBJ * eobj) {
	
	eobj->m_skeleton = std::make_unique<Skeleton>();
	
	if(eobj->grouplist.empty()) {
		// If no groups were specified
		
		eobj->m_skeleton->bones.resize(1);
		eobj->m_boneVertices.resize(1);
		
		Bone & bone = eobj->m_skeleton->bones.front();
		auto & vertices = eobj->m_boneVertices.front();
		
		// Add all vertices to the bone
		for(VertexId vertex : eobj->vertexlist.handles()) {
			vertices.push_back(vertex);
		}
		
		bone.father = { };
		bone.anim.scale = Vec3f(1.f);
		
	} else {
		// Groups were specified
		
		eobj->m_skeleton->bones.resize(eobj->grouplist.size());
		eobj->m_boneVertices.resize(eobj->grouplist.size());
		
		// Create one bone for each vertex group and assign vertices to the inner-most group
		util::HandleVector<VertexId, bool> vertexAssigned(eobj->vertexlist.size(), false);
		for(VertexGroupId i : eobj->grouplist.handles() | boost::adaptors::reversed) {
			
			const VertexGroup & group = eobj->grouplist[i];
			Bone & bone = eobj->m_skeleton->bones[i];
			auto & vertices = eobj->m_boneVertices[i];
			
			for(VertexId vertex : group.indexes) {
				if(!vertexAssigned[vertex]) {
					vertexAssigned[vertex] = true;
					vertices.push_back(vertex);
				}
			}
			
			bone.anim.trans = eobj->vertexlist[group.origin].v;
			bone.father = getParentGroup(eobj, i);
			arx_assert(!bone.father || size_t(bone.father) < size_t(i));
			bone.anim.scale = Vec3f(1.f);
			
		}
		
		// Assign vertices that are not in any group to the root bone
		for(VertexId vertex : eobj->vertexlist.handles()) {
			if(!getGroupForVertex(eobj, vertex)) {
				eobj->m_boneVertices.front().push_back(vertex);
			}
		}
		
		// Calculate relative bone positions
		for(Bone & bone : eobj->m_skeleton->bones) {
			if(bone.father) {
				const Bone & parent = eobj->m_skeleton->bones[bone.father];
				bone.transinit_global = bone.init.trans = bone.anim.trans - parent.anim.trans;
			} else {
				bone.transinit_global = bone.init.trans = bone.anim.trans;
			}
		}
		
	}
	
	// Calculate relative vertex positions
	eobj->vertexlocal.resize(eobj->vertexlist.size());
	for(VertexGroupId group : eobj->m_skeleton->bones.handles()) {
		for(VertexId vertex : eobj->m_boneVertices[group]) {
			eobj->vertexlocal[vertex] = eobj->vertexlist[vertex].v - eobj->m_skeleton->bones[group].anim.trans;
		}
	}
	
}

LODFlag strToLOD(std::string str, std::string strDefault) {
	strDefault = util::toLowercase(strDefault);
	str = util::toLowercase(str);
	LODFlag lt = LOD_PERFECT;
	for(int i = 0; i < 2; i++) {
		if(str == "perfect") lt = LOD_PERFECT;
		else
		if(str == "high"   ) lt = LOD_HIGH;
		else
		if(str == "medium" ) lt = LOD_MEDIUM;
		else
		if(str == "low"    ) lt = LOD_LOW;
		else
		if(str == "bad"    ) lt = LOD_BAD;
		else
		if(str == "flat"   ) lt = LOD_FLAT;
		else {
			arx_assert_msg(strDefault != "invalid", "Invalid default LOD '%s'", str.c_str());
			
			LogWarning << "fixing invalid LOD '" << str << "' to '" << strDefault << "'";
			str = strDefault;
			strDefault = "invalid";
			continue;
		}
		
		break;
	}
	return lt;
}
res::path fix3DModelFilename(Entity & io, const res::path & fileRequest) {
	//PakFile * pf = g_resources->getFile(io.usemesh);
	//if(pf) return io.usemesh;
	//if(io.obj) {
		//pf = g_resources->getFile(io.obj->fileUniqueRelativePathName);
		//if(pf) return io.obj->fileUniqueRelativePathName;
	//}
	
	// TODO all below may be unnecessary...
	res::path fileOk;
	std::string strErrMsg;
	std::ifstream fileValidate;
	char cCheck;
	std::vector<std::string> vFiles; // priority is by probable request
	vFiles.push_back(fileRequest.string());
	vFiles.push_back(io.usemesh.string());
	if(io.obj) {
		vFiles.push_back(io.obj->fileUniqueRelativePathName.string());
		vFiles.push_back(io.obj->file.string());
	}
	bool bCanMsg = false;
	for(std::string strFl : vFiles) {
		if(strFl.size() == 0) continue;
		bCanMsg = true;
		//LogWarning << "trying: " << strFl; // comment
		LogDebug(strFl);
		if(boost::starts_with(strFl, "graph/")) {
			fileValidate.open((std::string() + "game/" + strFl).c_str(), std::ifstream::in);
		} else {
			fileValidate.open(strFl.c_str(), std::ifstream::in);
		}
		cCheck = fileValidate.get();
		if(fileValidate.good()) {
			fileOk = strFl;
			fileValidate.close();
			break;
		} else {
			strErrMsg += " '" + strFl + "'" + (cCheck = '.');
		}
	}
	
	if(bCanMsg && io.obj->fileUniqueRelativePathName.string().size() == 0) { // this means the main model was never loaded before, so this could be the first time TODO any better hint?
		bCanMsg = false;
	}
	
	if(bCanMsg && fileOk.string().size() == 0) {
		LogError << "3D Model not found for " << io.idString() << " (all filenames should be lower case). Failed: " << strErrMsg;
	}
	
	return fileOk;
}
bool load3DModelAndLOD(Entity & io, const res::path & fileRequest, bool pbox) { // TODO if this works, try to substitute everywhere using loadObject() for items at least, but only where the returned unique_ptr is release() !
	static std::vector<LODFlag> ltOrderedList = {LOD_PERFECT, LOD_HIGH, LOD_MEDIUM, LOD_LOW, LOD_BAD, LOD_FLAT}; // best to worst
	
	res::path fileOk = fix3DModelFilename(io, fileRequest);
	if(fileOk.string().size() == 0) return false;
	
	res::path fileChkLOD;
	std::string strLOD;
	for(LODFlag ltChkLOD : ltOrderedList) {
		//if(ltChkLOD < ltMax) continue; // TODO limit LOD loading?
		//if(ltChkLOD > ltMin) continue;
		
		fileChkLOD = fileOk;
		strLOD = "";
		
		switch(ltChkLOD) {
			case LOD_PERFECT: break;
			case LOD_HIGH:    strLOD = "[LODH]"; break;
			case LOD_MEDIUM:  strLOD = "[LODM]"; break;
			case LOD_LOW:     strLOD = "[LODL]"; break;
			case LOD_BAD:     strLOD = "[LODB]"; break;
			case LOD_FLAT:    strLOD = "[LODF]"; break;
			default: arx_assert_msg(false, "not implemented LOD %d", ltChkLOD); break;
		}
		
		if(strLOD.size() > 0) {
			fileChkLOD.remove_ext().append( util::toLowercase(strLOD) ).append( fileOk.ext() );
		}
		
		if(io.obj && io.obj->fileUniqueRelativePathName == fileChkLOD && io.objLOD[ltChkLOD] == nullptr) {
			io.objLOD[ltChkLOD] = io.obj;
		} else {
			EERIE_3DOBJ * objLoad = loadObject(fileChkLOD, pbox).release();
			if(objLoad) {
				io.objLOD[ltChkLOD] = objLoad;
				if(!io.obj) { // default becomes best quality available
					io.obj = objLoad;
					io.currentLOD = ltChkLOD;
				} else {
					if(io.currentLOD == ltChkLOD && io.obj->fileUniqueRelativePathName.basename() != fileChkLOD.basename()) {
						LogWarning << "3DModel basenames for " << io.idString() << " differ objFile=" << io.obj->fileUniqueRelativePathName << " fileLOD=" << fileChkLOD << " "; // TODO LogDebug
					}
				}
			}
		}
		
		if(io.objLOD[ltChkLOD]) {
			io.availableLODFlags |= ltChkLOD;
		}
	}
	
	if(!io.obj) {
		LogError << "3D Model not found for " << io.idString() << " '" << fileRequest.string() << "' (pbox:" << pbox << ")";
		return false;
	}
	
	#ifdef ARX_DEBUG
	if(io.usemesh.string().size() > 0 && io.obj->fileUniqueRelativePathName.string().size() > 0 && io.usemesh != io.obj->fileUniqueRelativePathName) {
		LogDebug("3DModel filenames for " << io.idString() << " differ objFile=" << io.obj->fileUniqueRelativePathName << " usemesh=" << io.usemesh << " ");
	}
	#endif
	
	return true;
}

std::unique_ptr<EERIE_3DOBJ> loadObject(const res::path & file, bool pbox) {
	
	std::unique_ptr<EERIE_3DOBJ> object = ARX_FTL_Load(file);
	if(object && pbox) {
		EERIE_PHYSICS_BOX_Create(object.get());
	}
	
	return object;
}

void EERIE_OBJECT_CenterObjectCoordinates(EERIE_3DOBJ * ret) {
	
	if(!ret) {
		return;
	}
	
	Vec3f offset = ret->vertexlist[ret->origin].v;
	if(offset == Vec3f(0.f)) {
		return;
	}
	
	LogWarning << "NOT CENTERED " << ret->file;
	
	for(EERIE_VERTEX & vertex : ret->vertexlist) {
		vertex.v -= offset;
	}
	
}