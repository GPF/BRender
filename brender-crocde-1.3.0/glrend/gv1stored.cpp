#include "drv_ip.hpp"
#include "brassert.h"
#include "formats.h"
#include "log.h"

#define ENABLE_FACE_GROUP_COUNT 1

#if DEBUG && ENABLE_FACE_GROUP_COUNT
extern "C" int faceGroupCount;
extern "C" int modelsDrawnCount;
extern "C" int trianglesRenderedCount;
extern "C" int trianglesDrawnCount;
extern "C" int verticesRenderedCount;
#endif


/*
* Geometry format info. template
*/
#define F(f) offsetof(br_geometry_stored_gl, f)

static const struct br_tv_template_entry templateEntries[] = {
	{BRT_IDENTIFIER_CSTR,	F(identifier),			BRTV_QUERY | BRTV_ALL,	BRTV_CONV_COPY,},
	{BRT_GEOMETRY_V1_MODEL,	F(gv1model),			BRTV_QUERY | BRTV_ALL,	BRTV_CONV_COPY,},
	{BRT_SHARED_B,			F(shared),				BRTV_QUERY | BRTV_ALL,	BRTV_CONV_COPY,},
};
#undef F


static const char * br_geometry_stored_gl_identifier(br_object *_self)
{
	br_geometry_stored_gl *self = reinterpret_cast<br_geometry_stored_gl*>(_self);
	return self->identifier;
}

static br_device * br_geometry_stored_gl_device(br_object *_self)
{
	br_geometry_stored_gl *self = reinterpret_cast<br_geometry_stored_gl*>(_self);
	return reinterpret_cast<br_device*>(self->device);
}

static void br_geometry_stored_gl_free(br_object *_self)
{
	br_geometry_stored_gl *self = reinterpret_cast<br_geometry_stored_gl*>(_self);
	ObjectContainerRemove(self->gv1model->renderer_facility, self);

	glDeleteVertexArrays(1, &self->glVao);
	glDeleteBuffers(1, &self->glVbo);
	glDeleteBuffers(1, &self->glIbo);
	BrResFreeNoCallback(self);
}

static br_token br_geometry_stored_gl_type(br_object *self)
{
	return BRT_GEOMETRY_STORED;
}

static br_boolean br_geometry_stored_gl_isType(br_object *self, br_token t)
{
	return (t == BRT_GEOMETRY_STORED) || (t == BRT_GEOMETRY) || (t == BRT_OBJECT);
}

static br_size_t br_geometry_stored_gl_space(br_object *self)
{
	return sizeof(br_geometry_stored_gl);
}

static struct br_tv_template * br_geometry_stored_gl_templateQuery(br_object *_self)
{
	br_geometry_stored_gl *self = reinterpret_cast<br_geometry_stored_gl*>(_self);
	if(self->device->templates.geometryStored == nullptr)
		self->device->templates.geometryStored = BrTVTemplateAllocate(self->device, templateEntries, BR_ASIZE(templateEntries));

	return self->device->templates.geometryStored;
}

static void applyStoredProperties(HVIDEO hVideo, HGLSTATE_STACK state, uint32_t states, bool& unlit, HGLSTD140_MODEL_DATA hModel, bool depthOverride)
{
	if(depthOverride)
		glDisable(GL_DEPTH_TEST);
	else
		glEnable(GL_DEPTH_TEST);

	glDisable(GL_BLEND);

	/* Only use the states we want (if valid). */
	states = state->valid & states;

	if(states & GLSTATE_MASK_CULL)
	{
		switch(state->cull.type)
		{
			case BRT_ONE_SIDED:
				glEnable(GL_CULL_FACE);
				glCullFace(GL_BACK);
				break;
			case BRT_TWO_SIDED:
				/* Don't cull, is handled in the shader. */
			case BRT_NONE:
			default:
				glDisable(GL_CULL_FACE);
				break;
		}
	}

	unlit = false;
	if(states & GLSTATE_MASK_SURFACE)
	{
		glActiveTexture(GL_TEXTURE0);

		if(state->surface.colour_source == BRT_SURFACE)
		{
			br_uint_32 colour = state->surface.colour;
			float r = BR_RED(colour) / 255.0f;
			float g = BR_GRN(colour) / 255.0f;
			float b = BR_BLU(colour) / 255.0f;
			hModel->surface_colour = glm::vec4(r, g, b, state->surface.opacity);
		}
		else
		{
			hModel->surface_colour = glm::vec4(1.0f, 1.0f, 1.0f, state->surface.opacity);
		}

		hModel->ka = state->surface.ka;
		hModel->ks = state->surface.ks;
		hModel->kd = state->surface.kd;
		hModel->power = state->surface.power;

		/* ambient_{r,g,b} are multiplied by the colour values. */
		hModel->ambient_colour = glm::vec4(state->surface.ambient_r, state->surface.ambient_g, state->surface.ambient_g, state->surface.opacity);
		unlit = !state->surface.lighting;
	}

	if(states & GLSTATE_MASK_PRIMITIVE)
	{
		if(state->prim.flags & PRIMF_DEPTH_WRITE)
			glDepthMask(GL_TRUE);
		else
			glDepthMask(GL_FALSE);

		if(state->prim.flags & PRIMF_COLOUR_WRITE)
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		else
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		if(state->prim.flags & PRIMF_BLEND)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);

		GLenum depthFunc;
		switch(state->prim.depth_test)
		{
			case BRT_LESS: depthFunc = GL_LESS; break;
			case BRT_GREATER: depthFunc = GL_GREATER; break;
			case BRT_LESS_OR_EQUAL: depthFunc = GL_LEQUAL; break;
			case BRT_GREATER_OR_EQUAL: depthFunc = GL_GEQUAL; break;
			case BRT_EQUAL: depthFunc = GL_EQUAL; break;
			case BRT_NOT_EQUAL: depthFunc = GL_NOTEQUAL; break;
			default: depthFunc = GL_LESS;
		}
		glDepthFunc(depthFunc);

		if(state->prim.colour_map)
		{
			glBindTexture(GL_TEXTURE_2D, state->prim.colour_map->glTex);
			glUniform1i(hVideo->brenderProgram.uniforms.main_texture, hVideo->brenderProgram.mainTextureBinding);

			//if(state->prim.colour_map->source->flags & BR_PMF_KEYED_TRANSPARENCY)
			//{
			//	//BrDebugBreak();
			//}
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, hVideo->texture.white);
			glUniform1i(hVideo->brenderProgram.uniforms.main_texture, hVideo->brenderProgram.mainTextureBinding);
		}

		GLenum minFilter, magFilter;
		GLfloat maxAnisotropy;
		if(state->prim.filter == BRT_LINEAR && state->prim.mip_filter == BRT_LINEAR) {
			minFilter = GL_LINEAR_MIPMAP_LINEAR;
			magFilter = GL_LINEAR;
			maxAnisotropy = hVideo->maxAnisotropy;
		} else if(state->prim.filter == BRT_LINEAR && state->prim.mip_filter == BRT_NONE) {
			minFilter = GL_LINEAR;
			magFilter = GL_LINEAR;
			maxAnisotropy = 1.0f;
		} else if(state->prim.filter == BRT_NONE && state->prim.mip_filter == BRT_LINEAR) {
			minFilter = GL_NEAREST_MIPMAP_NEAREST;
			magFilter = GL_NEAREST;
			maxAnisotropy = hVideo->maxAnisotropy;
		} else if(state->prim.filter == BRT_NONE && state->prim.mip_filter == BRT_NONE) {
			minFilter = GL_NEAREST;
			magFilter = GL_NEAREST;
			maxAnisotropy = 1.0f;
		} else {
			assert(0);
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

		if(ogl_ext_EXT_texture_filter_anisotropic)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);

		//if(state->prim.colour_map)
		//{
		//	if(state->prim.colour_map->source)
		//	{
		//		if(state->prim.colour_map->source->identifier)
		//		{
		//			if(!stricmp("sprkwt02", state->prim.colour_map->source->identifier))
		//			{
		//				int x = 0;
		//			}
		//		}
		//	}
		//}

		/* This seems to be the only way to see if something needs blending :/ */
		if(state->prim.index_blend != nullptr)
		{
			glEnable(GL_BLEND);

			glBlendEquation(GL_FUNC_ADD);
			if(state->prim.colour_map != nullptr)
			{
				/* https://stackoverflow.com/a/37533711 */
				if(state->prim.colour_map->source_flags & BR_PMF_PREMULTIPLIED_ALPHA)
					glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				else
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
			else
			{
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
		}
	}
}


static render_mode getRenderMode(HGLSTATE_STACK state)
{
	if(state->hidden.type == BRT_BUCKET_SORT)
		return render_mode::opaque;

	if(state->valid & GLSTATE_MASK_SURFACE)
	{
		if(state->surface.force_back)
			return render_mode::force_back;
		
		if(state->surface.force_front)
			return render_mode::force_front;

		/* Transparent? Defer. */
		if(state->surface.opacity < BR_SCALAR(1.0f))
			return render_mode::transparent;
	}

	if(state->valid & GLSTATE_MASK_PRIMITIVE)
	{
		/* Blend flags set? Defer.*/
		if(state->prim.flags & PRIMF_BLEND)
			return render_mode::transparent;

		/* Has a blend table? Defer. */
		if(state->prim.index_blend != nullptr)
			return render_mode::transparent;

		if(state->prim.colour_map && state->prim.colour_map->source)
		{
			/* Keyed transparency? Defer. */
			if(state->prim.colour_map->source->flags & BR_PMF_KEYED_TRANSPARENCY)
				return render_mode::transparent;
		}
	}

	return render_mode::opaque;
}


static void calcRenderModes(br_geometry_stored_gl *self, br_renderer_gl *renderer, br_renderer_gl_deferred *d)
{
	render_mode defaultMode = getRenderMode(renderer->state.current);

	for(size_t i = 0; i < self->model->ngroups; ++i)
	{
		br_renderer_state_stored_gl *stored = (br_renderer_state_stored_gl*)self->model->groups[i].stored;

		render_mode mode = stored ? getRenderMode(&stored->state) : defaultMode;

		d->flags[static_cast<int>(mode)].set(i, true);
	}
}


/*
** Render a bunch of model groups.
** Group //i// will be rendered if flag //i// is set in //rflags//.
*/
void StoredGLRenderGroups(br_geometry_stored_gl *self, br_renderer_gl *renderer, grflags rflags, bool depthOverride)
{
#if DEBUG && ENABLE_FACE_GROUP_COUNT
	modelsDrawnCount++;
#endif

	HGLCACHE hCache = &renderer->state.cache;

	/* Update the per-model cache (matrices and lights) */
	GLCACHE_UpdateModel(hCache, &renderer->state.current->matrix);

	HVIDEO hVideo = &self->gv1model->device->video;

#if DEBUG
	{ /* Check that sceneBegin() actually did it's shit. */

		/* Program */
		GLint p;
		glGetIntegerv(GL_CURRENT_PROGRAM, &p);
		ASSERT(p == hVideo->brenderProgram.program);

		/* FBO */
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &p);
		ASSERT(p == renderer->state.current->output.colour->asBack.glFbo);

		/* Model UBO */
		glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &p);
		ASSERT(p == hVideo->brenderProgram.uboModel);
	}
#endif

	GLSTD140_MODEL_DATA model;
	model.projection = hCache->model.p;
	model.model_view = hCache->model.mv;
	model.mvp = hCache->model.mvp;
	model.normal_matrix = hCache->model.normal;

	//assert(glGetError() == GL_NO_ERROR);

	glBindVertexArray(self->glVao);

	bool modelLit = self->model->flags & V11MODF_LIT;

	/* For each group, */
	uintptr_t offset = 0;
	for(int i = 0; i < self->model->ngroups; offset += self->model->groups[i].nfaces * 3 * sizeof(br_uint_16), ++i)
	{
		/* Ignore unflagged groups */
		if(!rflags[i])
			continue;

		br_renderer_state_stored_gl *stored = reinterpret_cast<br_renderer_state_stored_gl*>(self->model->groups[i].stored);

		bool unlit = true;
		if(stored)
		{
			bool unlitStored = true, unlitGlobal = true;

			/* Attempt 1:
			** If the group has stored state, apply primitive, surface, and cull states from it, and everything
			** else from global.
			*/
			//applyStoredProperties(hVideo, &stored->state, GLSTATE_MASK_PRIMITIVE | GLSTATE_MASK_SURFACE | GLSTATE_MASK_CULL, unlitStored);
			//applyStoredProperties(hVideo, &renderer->state.current, ~(GLSTATE_MASK_PRIMITIVE | GLSTATE_MASK_SURFACE | GLSTATE_MASK_CULL), unlitGlobal);


			/* Attempt 2:
			** If the group has stored state, apply everything from global and then patch it with the group's state.
			** NB: It might be more efficient to create a new GLSTATE_STACK and apply them in-memory than to applyStoredProperties()
			** twice.
			*/
			//applyStoredProperties(hVideo, &renderer->state.current, ~0, unlitStored);
			//applyStoredProperties(hVideo, &stored->state, GLSTATE_MASK_PRIMITIVE | GLSTATE_MASK_SURFACE | GLSTATE_MASK_CULL, unlitGlobal);

			/* Attempt 3:
			** I was right in my very early attempts (before attempt 1). Just apply our stored state, nothing else matters.
			*/
			applyStoredProperties(hVideo, &stored->state, GLSTATE_MASK_PRIMITIVE | GLSTATE_MASK_SURFACE | GLSTATE_MASK_CULL, unlitGlobal, &model, depthOverride);

			//unlit = !modelLit;// || unlitGlobal;// unlitStored || unlitGlobal;
			unlit = unlitGlobal;// unlitStored || unlitGlobal;
			//unlit = !modelLit;

		}
		else
		{
			/* If there's no stored state, apply all states from global. */
			applyStoredProperties(hVideo, renderer->state.current, ~0u, unlit, &model, depthOverride);
		}

		model.unlit = static_cast<uint32_t>(unlit);
		model.clear_colour = glm::make_vec4(renderer->pixelmap->asBack.clearColour);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(model), &model);
		glDrawElements(GL_TRIANGLES, self->model->groups[i].nfaces * 3, GL_UNSIGNED_SHORT, reinterpret_cast<void*>(offset));

#if DEBUG && ENABLE_FACE_GROUP_COUNT
		faceGroupCount++;
		trianglesRenderedCount += self->model->groups[i].nfaces;
		trianglesDrawnCount += self->model->groups[i].nfaces;
		verticesRenderedCount += self->model->groups[i].nfaces * 3;
#endif
	}
}

static float getDistanceFromZero(const br_renderer_gl_deferred& a)
{
	glm::mat4x4 mat = VIDEOI_BrMatrix34ToGLM44(&a.snapshot.matrix.model_to_view);
	return glm::distance(glm::vec3(), glm::vec3(mat[3]));
}

br_error br_geometry_stored_gl_renderOnScreen(struct br_geometry_stored *_self, struct br_renderer *_renderer)
{
	br_geometry_stored_gl *self = reinterpret_cast<br_geometry_stored_gl*>(_self);
	br_renderer_gl *renderer = reinterpret_cast<br_renderer_gl*>(_renderer);
	HVIDEO hVideo = &self->gv1model->device->video;

	/* Call sceneBegin() if we haven't already. I hate doing it here. */
	if(!renderer->has_begun)
		RendererGLSceneBegin(_renderer);

	/* Initial render pass. Render all opaque geometry.
	** If rendering with buckets, defer any transparent geometry.
	** Otherwise, draw it all!
	*/
	if(renderer->state.current->hidden.type == BRT_NONE)
	{
		/* No buckets, defer nothing. */
		grflags f;
		f.flip();
		StoredGLRenderGroups(self, renderer, f, false);
	}
	else
	{
		/*
		** Initial render pass. Don't actually render anything yet, but sort it
		** by distance from camera (at (0,0,0)) and defer any transparent.
		** See gv1buckt.cpp for the actual drawing.
		*/
		br_renderer_gl_deferred de;
		de.snapshot = *renderer->state.current;
		de.stored = self;
		de.distance = getDistanceFromZero(de);
		calcRenderModes(self, renderer, &de);
		renderer->geom.insert(de);
	}

	return BRE_OK;
}

br_error br_geometry_stored_gl_render(struct br_geometry_stored *_self, struct br_renderer *_renderer)
{
	/* Oh, we're partially visible? Cool, just render all of us. */
	return br_geometry_stored_gl_renderOnScreen(_self, _renderer);
}

static const struct br_geometry_stored_dispatch geometryStoredDispatch = {
	.__reserved0		= nullptr,
	.__reserved1		= nullptr,
	.__reserved2		= nullptr,
	.__reserved3		= nullptr,
	._free				= br_geometry_stored_gl_free,
	._identifier		= br_geometry_stored_gl_identifier,
	._type				= br_geometry_stored_gl_type,
	._isType			= br_geometry_stored_gl_isType,
	._device			= br_geometry_stored_gl_device,
	._space				= br_geometry_stored_gl_space,

	._templateQuery		= br_geometry_stored_gl_templateQuery,
	._query				= BR_CMETHOD(br_object, query),
	._queryBuffer		= BR_CMETHOD(br_object, queryBuffer),
	._queryMany			= BR_CMETHOD(br_object, queryMany),
	._queryManySize		= BR_CMETHOD(br_object, queryManySize),
	._queryAll			= BR_CMETHOD(br_object, queryAll),
	._queryAllSize		= BR_CMETHOD(br_object, queryAllSize),

	._render			= br_geometry_stored_gl_render,
	._renderOnScreen	= br_geometry_stored_gl_renderOnScreen
};


typedef struct
{
	br_vector3_f p;			/* Point in model space				*/
	br_vector2_f map;		/* Mapping coordinates				*/
	br_vector3_f n;		    /* Surface normal at vertex			*/
	br_colour c;			/* Colour							*/
} gl_vertex_f;


static void createVAO(br_geometry_stored_gl *self)
{
	HVIDEO hVideo = &self->gv1model->device->video;

	glGenVertexArrays(1, &self->glVao);
	glGenBuffers(1, &self->glVbo);
	glGenBuffers(1, &self->glIbo);

	glBindVertexArray(self->glVao);

	{ /* gl_vertex_f */
		glBindBuffer(GL_ARRAY_BUFFER, self->glVbo);

		glEnableVertexAttribArray(hVideo->brenderProgram.attributes.aPosition);
		glVertexAttribPointer(hVideo->brenderProgram.attributes.aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_f), (void*)offsetof(gl_vertex_f, p));

		glEnableVertexAttribArray(hVideo->brenderProgram.attributes.aUV);
		glVertexAttribPointer(hVideo->brenderProgram.attributes.aUV, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_f), (void*)offsetof(gl_vertex_f, map));

		glEnableVertexAttribArray(hVideo->brenderProgram.attributes.aNormal);
		glVertexAttribPointer(hVideo->brenderProgram.attributes.aNormal, 3, GL_FLOAT, GL_FALSE, sizeof(gl_vertex_f), (void*)offsetof(gl_vertex_f, n));

		glVertexAttribPointer(hVideo->brenderProgram.attributes.aColour, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_vertex_f), (void*)offsetof(gl_vertex_f, c));
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->glIbo);

	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void buildVbo(br_geometry_stored_gl *self, struct v11model_f *model, size_t totalVertices)
{
	/* Collate and upload the vertex data. */
	gl_vertex_f *vtx = (gl_vertex_f*)BrScratchAllocate(totalVertices * sizeof(gl_vertex_f));
	ASSERT(vtx);
	gl_vertex_f *nextVtx = vtx;
	for(br_uint_16 i = 0; i < model->ngroups; ++i)
	{
		for(br_uint_16 v = 0; v < model->groups[i].nvertices; ++v, ++nextVtx)
		{
			nextVtx->p = model->groups[i].vertices[v].p;
			nextVtx->map = model->groups[i].vertices[v].map;
			nextVtx->n = model->groups[i].vertices[v].n;
			nextVtx->c = model->groups[i].vertex_colours[v];
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, self->glVbo);
	glBufferData(GL_ARRAY_BUFFER, totalVertices * sizeof(gl_vertex_f), vtx, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	BrScratchFree(vtx);
}

static void buildIbo(br_geometry_stored_gl *self, struct v11model_f *model, size_t totalFaces)
{
	br_uint_16 *idx = (br_uint_16*)BrScratchAllocate(totalFaces * 3 * sizeof(br_uint_16));

	br_uint_16 *nextIdx = idx;
	br_uint_16 offset = 0;
	for(br_uint_16 i = 0; i < model->ngroups; ++i)
	{
		for(br_uint_16 f = 0; f < model->groups[i].nfaces; ++f)
		{
			*nextIdx++ = model->groups[i].faces[f].vertices[0] + offset;
			*nextIdx++ = model->groups[i].faces[f].vertices[1] + offset;
			*nextIdx++ = model->groups[i].faces[f].vertices[2] + offset;
		}
		offset += model->groups[i].nvertices;
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->glIbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, totalFaces * 3 * sizeof(br_uint_16), idx, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	BrScratchFree(idx);
}

br_geometry_stored_gl *GeometryStoredGLAllocate(br_geometry_v1_model_gl *gv1model, const char *id, struct v11model_f *model)
{
	br_geometry_stored_gl *self = BrResAllocate<br_geometry_stored_gl>(gv1model->renderer_facility->object_list,  BR_MEMORY_OBJECT);

	self->dispatch = (struct br_geometry_stored_dispatch *)&geometryStoredDispatch;
	self->identifier = id;
	self->device = gv1model->device;
	self->gv1model = gv1model;

	ObjectContainerAddFront(gv1model->renderer_facility, self);

	self->model = model;
	self->shared = true;

	createVAO(self);

	size_t totalVertices = 0;
	size_t totalFaces = 0;

	for(br_uint_16 i = 0; i < model->ngroups; ++i)
	{
		totalVertices += model->groups[i].nvertices;
		totalFaces += model->groups[i].nfaces;
	}

	buildVbo(self, model, totalVertices);
	buildIbo(self, model, totalFaces);

	return self;
}

bool std::greater<br_renderer_gl_deferred>::operator()(const br_renderer_gl_deferred& a, const br_renderer_gl_deferred& b) const
{
	return a.distance > b.distance;
}
