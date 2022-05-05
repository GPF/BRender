#include <stddef.h>
#include <string.h>

#include "drv_ip.hpp"

/*
* Geometry format info. template
*/
#define F(f)    offsetof(struct br_geometry_lighting_gl, f)

static const struct br_tv_template_entry templateEntries[] = {
	{BRT_IDENTIFIER_CSTR,		F(identifier),			BRTV_QUERY | BRTV_ALL,	BRTV_CONV_COPY,},
	{BRT_RENDERER_FACILITY_O,	F(renderer_facility),	BRTV_QUERY | BRTV_ALL,	BRTV_CONV_COPY,},
	{BRT_FACILITY_O,			F(renderer_facility),	BRTV_QUERY,				BRTV_CONV_COPY,},
};
#undef F

static const char *BR_CMETHOD_DECL(br_geometry_lighting_gl, identifier)(br_object *_self)
{
	return reinterpret_cast<br_geometry_lighting_gl*>(_self)->identifier;
}

static br_device *BR_CMETHOD_DECL(br_geometry_lighting_gl, device)(br_object *_self)
{
	br_geometry_lighting_gl *self = reinterpret_cast<br_geometry_lighting_gl*>(_self);
	return reinterpret_cast<br_device*>(self->device);
}

static void BR_CMETHOD_DECL(br_geometry_lighting_gl, free)(br_object *_self)
{
	br_geometry_lighting_gl *self = reinterpret_cast<br_geometry_lighting_gl*>(_self);
	ObjectContainerRemove(self->renderer_facility, self);
	BrResFreeNoCallback(self);
}

static br_token BR_CMETHOD_DECL(br_geometry_lighting_gl, type)(br_object *self)
{
	return BRT_GEOMETRY_LIGHTING;
}

static br_boolean BR_CMETHOD_DECL(br_geometry_lighting_gl, isType)(br_object *self, br_token t)
{
	return (t == BRT_GEOMETRY_LIGHTING) || (t == BRT_GEOMETRY) || (t == BRT_OBJECT);
}

static br_size_t BR_CMETHOD_DECL(br_geometry_lighting_gl, space)(br_object *self)
{
	return sizeof(br_geometry_lighting);
}

static struct br_tv_template *BR_CMETHOD_DECL(br_geometry_lighting_gl, templateQuery)(br_object *_self)
{
	br_geometry_lighting_gl *self = reinterpret_cast<br_geometry_lighting_gl*>(_self);

	if(self->device->templates.geometryLighting == nullptr)
		self->device->templates.geometryLighting = BrTVTemplateAllocate(self->device, templateEntries, BR_ASIZE(templateEntries));

	return self->device->templates.geometryLighting;
}

br_error BR_CMETHOD_DECL(br_geometry_lighting_gl, render)
(struct br_geometry_lighting *self, struct br_renderer *renderer,
 br_vector3_f *points, br_vector3_f *normals,
 br_colour *colour_in, br_colour *colour_out,
 br_uint_16 *redirect, int pstride, int nstride,
 int cinstride, int coutstride, int nvertices)
{
	return BRE_FAIL;
}

/*
* Default dispatch table for renderer type
*/
static const struct br_geometry_lighting_dispatch geometryLightingDispatch = {
	.__reserved0	= nullptr,
	.__reserved1	= nullptr,
	.__reserved2	= nullptr,
	.__reserved3	= nullptr,
	._free			= BR_CMETHOD(br_geometry_lighting_gl,	free),
	._identifier	= BR_CMETHOD(br_geometry_lighting_gl,	identifier),
	._type			= BR_CMETHOD(br_geometry_lighting_gl,	type),
	._isType		= BR_CMETHOD(br_geometry_lighting_gl,	isType),
	._device		= BR_CMETHOD(br_geometry_lighting_gl,	device),
	._space			= BR_CMETHOD(br_geometry_lighting_gl,	space),

	._templateQuery	= BR_CMETHOD(br_geometry_lighting_gl,	templateQuery),
	._query			= BR_CMETHOD(br_object,					query),
	._queryBuffer	= BR_CMETHOD(br_object,					queryBuffer),
	._queryMany		= BR_CMETHOD(br_object,					queryMany),
	._queryManySize	= BR_CMETHOD(br_object,					queryManySize),
	._queryAll		= BR_CMETHOD(br_object,					queryAll),
	._queryAllSize	= BR_CMETHOD(br_object,					queryAllSize),

	._render		= BR_CMETHOD(br_geometry_lighting_gl,	render),
};


/*
* Allocate a geometry format
*/
br_geometry_lighting_gl *GeometryLightingGLAllocate(br_renderer_facility_gl *type, const char *id)
{
	br_geometry_lighting_gl *self = BrResAllocate<br_geometry_lighting_gl>(type->object_list, BR_MEMORY_OBJECT);

	self->dispatch = &geometryLightingDispatch;
	self->identifier = id;
	self->device = type->output_facility->device;
	self->renderer_facility = type;

	ObjectContainerAddFront(type, self);
	return self;
}
