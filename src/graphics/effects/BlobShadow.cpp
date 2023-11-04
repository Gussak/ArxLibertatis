/*
 * Copyright 2016-2022 Arx Libertatis Team (see the AUTHORS file)
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

#include "graphics/effects/BlobShadow.h"

#include <array>
#include <iostream>
#include <sstream>
#include <vector>

#include <boost/range/adaptor/strided.hpp>

#include "game/Entity.h"
#include "graphics/Draw.h"
#include "graphics/GlobalFog.h"
#include "graphics/Renderer.h"
#include "graphics/particle/ParticleTextures.h"
#include "io/log/Logger.h"
#include "platform/profiler/Profiler.h"
#include "scene/Interactive.h"
#include "scene/Tiles.h"

static std::vector<TexturedVertex> g_shadowBatch;

static void addShadowBlob(const Entity & entity, Vec3f pos, float scale, bool isGroup) {
	
	EERIEPOLY * ep = CheckInPoly(pos);
	if(!ep) {
		return;
	}
	
	Vec3f in;
	in.y = ep->min.y - 3.f;
	
	float strength = (isGroup ? 0.8f : 0.5f) - glm::abs(pos.y - in.y) * 0.002f;
	strength = isGroup ? (strength * scale - entity.invisibility) : (strength - entity.invisibility) * scale;
	if(strength <= 0.f) {
		return;
	}
	
	float size = (isGroup ? 44.f :  16.f) * scale;
	in.x = pos.x - size * 0.5f;
	in.z = pos.z - size * 0.5f;
	std::array<Vec3f, 4> p = { in, in + Vec3f(size, 0, 0), in + Vec3f(size, 0, size), in + Vec3f(0, 0, size) };
	if(!isGroup && (p[0].z <= 0.f || p[1].z <= 0.f || p[2].z <= 0.f)) {
		return;
	}
	
	ColorRGBA color = Color::gray(strength).toRGB();
	std::array<TexturedVertex, 4> vertices = { {
		{ worldToClipSpace(p[0]), color, Vec2f(0.3f, 0.3f) },
		{ worldToClipSpace(p[1]), color, Vec2f(0.7f, 0.3f) },
		{ worldToClipSpace(p[2]), color, Vec2f(0.7f, 0.7f) },
		{ worldToClipSpace(p[3]), color, Vec2f(0.3f, 0.7f) }
	} };
	
	g_shadowBatch.push_back(vertices[0]);
	g_shadowBatch.push_back(vertices[2]);
	g_shadowBatch.push_back(vertices[1]);
	
	g_shadowBatch.push_back(vertices[0]);
	g_shadowBatch.push_back(vertices[3]);
	g_shadowBatch.push_back(vertices[2]);
	
}

#ifdef ARX_DEBUG_SHADOWBLOB
 #define LogDebug2(...) std::cerr<<__VA_ARGS__<<std::endl //TODO until I find a way to let debug log show up (preferably with a SHADOWBLOB filter!!) on the terminal using the run parameters
#else
 #define LogDebug2(...) ARX_DISCARD(__VA_ARGS__)
#endif
static int iSBShowLogCountDown=1;
static int siMaxShadowBlobCountForVertexes=-1;
static int siMaxShadowBlobCountForVertGrps=-1;
static void debugShadowBlob() {
  iSBShowLogCountDown--;
  if(iSBShowLogCountDown<0)iSBShowLogCountDown=60*10; //like every 10s if FPS is 60
  
  if(siMaxShadowBlobCountForVertexes==-1){
    char* pc=std::getenv("ARX_LIMIT_SHADOWBLOB_FOR_VERTEXES"); //RECOMENDED is: export ARX_LIMIT_SHADOWBLOB_FOR_VERTEXES=9
    if(pc==NULL){
      siMaxShadowBlobCountForVertexes=99999; //just any absurd high value to mean no limit
    }else{
      std::stringstream ss;
      ss << std::dec << pc;
      ss >> siMaxShadowBlobCountForVertexes;
    }
    LogDebug2("ARX_LIMIT_SHADOWBLOB_FOR_VERTEXES="<<siMaxShadowBlobCountForVertexes);
  }
  if(siMaxShadowBlobCountForVertGrps==-1){
    char* pc=std::getenv("ARX_LIMIT_SHADOWBLOB_FOR_VERTGRPS");
    if(pc==NULL){
      siMaxShadowBlobCountForVertGrps=99999; //just any absurd high value to mean no limit
    }else{
      std::stringstream ss;
      ss << std::dec << pc;
      ss >> siMaxShadowBlobCountForVertGrps;
    }
    LogDebug2("ARX_LIMIT_SHADOWBLOB_FOR_VERTGRPS="<<siMaxShadowBlobCountForVertGrps);
  }
}

void ARXDRAW_DrawInterShadows() {
	
	ARX_PROFILE_FUNC();
	
	g_shadowBatch.clear();
	
#ifdef ARX_DEBUG_SHADOWBLOB
  debugShadowBlob();
  if(iSBShowLogCountDown==0)LogDebug2(">>>>>>>>>>>>>>>>>>>> treatio.size()="<<treatio.size()<<" <<<<<<<<<<<<<<<<<<");
#endif
  if(false){
	for(const auto & entry : treatio) {
		
		if(entry.show != SHOW_FLAG_IN_SCENE || !entry.io) {
			continue;
		}
		
		const Entity & entity = *entry.io;
		if(!entity.obj || (entity.ioflags & IO_JUST_COLLIDE) || (entity.ioflags & IO_NOSHADOW)
		   || (entity.ioflags & IO_GOLD) || entity.show != SHOW_FLAG_IN_SCENE) {
			continue;
		}
		
		if(!g_tiles->isInActiveTile(entity.pos)) {
			continue;
		}
		
#ifdef ARX_DEBUG_SHADOWBLOB
    int iMaxShadowBlobCountForVertexes=siMaxShadowBlobCountForVertexes;
    int iMaxShadowBlobCountForVertGrps=siMaxShadowBlobCountForVertGrps;
    int iSBCount=0;
#endif
		if(entity.obj->grouplist.size() > 1) {
			for(const VertexGroup & group : entity.obj->grouplist) {
#ifdef ARX_DEBUG_SHADOWBLOB
        iMaxShadowBlobCountForVertGrps--;if(iMaxShadowBlobCountForVertGrps<0)break;
        iSBCount++;
#endif
				addShadowBlob(entity, entity.obj->vertexWorldPositions[group.origin].v, group.m_blobShadowSize, true);
			}
      if(iSBShowLogCountDown==0)LogDebug2("ShadowBlob("<<entity.idString()<<"):grouplist.size()="<<entity.obj->grouplist.size()<<",iSBCount="<<iSBCount);
		} else {
			for(const EERIE_VERTEX & vertex : entity.obj->vertexWorldPositions | boost::adaptors::strided(9)) {
#ifdef ARX_DEBUG_SHADOWBLOB
        iMaxShadowBlobCountForVertexes--;if(iMaxShadowBlobCountForVertexes<0)break;
        iSBCount++;
#endif
				addShadowBlob(entity, vertex.v, entity.scale, false);
			}
#ifdef ARX_DEBUG_SHADOWBLOB
      if(iSBShowLogCountDown==0)LogDebug2("ShadowBlob("<<entity.idString()<<"):vertexWorldPositions.size()="<<entity.obj->vertexWorldPositions.size()<<"(WouldRequest:"<<(entity.obj->vertexWorldPositions | boost::adaptors::strided(9)).size()<<"),iSBCount="<<iSBCount);
#endif
		}
	}
  }else{
	for(const auto & entry : treatio) {
		
		if(entry.show != SHOW_FLAG_IN_SCENE || !entry.io) {
			continue;
		}
		
		const Entity & entity = *entry.io;
		if(!entity.obj || (entity.ioflags & IO_JUST_COLLIDE) || (entity.ioflags & IO_NOSHADOW)
		   || (entity.ioflags & IO_GOLD) || entity.show != SHOW_FLAG_IN_SCENE) {
			continue;
		}
		
		if(!g_tiles->isInActiveTile(entity.pos)) {
			continue;
		}
		
		if(entity.obj->grouplist.size() > 1) {
			for(const VertexGroup & group : entity.obj->grouplist) {
				addShadowBlob(entity, entity.obj->vertexWorldPositions[group.origin].v, group.m_blobShadowSize, true);
			}
		} else {
      int iSBLimit = entity.obj->vertexWorldPositions.size() > 100 ? 9 : INT_MAX; //high poly item's models would create a very dark dense shadow otherwise. INT_MAX just means no limit.
			for(const EERIE_VERTEX & vertex : entity.obj->vertexWorldPositions | boost::adaptors::strided(9)) {
        iSBLimit--;
        if(iSBLimit<0)break;
				addShadowBlob(entity, vertex.v, entity.scale, false);
			}
		}
	}
  }
  	
	if(!g_shadowBatch.empty()) {
		GRenderer->SetFogColor(Color());
		UseRenderState state(render3D().depthWrite(false).blend(BlendZero, BlendInvSrcColor).depthOffset(1));
		GRenderer->SetTexture(0, g_particleTextures.boom);
		EERIEDRAWPRIM(Renderer::TriangleList, g_shadowBatch.data(), g_shadowBatch.size());
		GRenderer->SetFogColor(g_fogColor);
	}
	
}
