#include "drv_ip.hpp"
#include "glstate_ip.hpp"

#define S BRTV_SET
#define Q BRTV_QUERY
#define A BRTV_ALL

#define AX 0
#define AF BRTV_ALL

#define F(f) offsetof(GLSTATE_STACK,f)

static const br_tv_template_entry GLSTATEI_LightTemplateEntries[] = {
	{BRT(TYPE_T),			F(light[0].type),			Q | S | A,	BRTV_CONV_COPY,				0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(SPACE_T),			F(light[0].lighting_space),	Q | S | A,	BRTV_CONV_COPY,				0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(POSITION_V3_X),	F(light[0].position),		Q | S | AX,	BRTV_CONV_V3_FIXED_SCALAR,	0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(POSITION_V3_F),	F(light[0].position),		Q | S | AF,	BRTV_CONV_V3_FLOAT_SCALAR,	0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(DIRECTION_V3_X),	F(light[0].direction),		Q | S | AX,	BRTV_CONV_V3_FIXED_SCALAR,	0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(DIRECTION_V3_F),	F(light[0].direction),		Q | S | AF,	BRTV_CONV_V3_FLOAT_SCALAR,	0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(SPOT_INNER_X),		F(light[0].spot_inner),		Q | S | AX,	BRTV_CONV_FIXED_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(SPOT_INNER_F),		F(light[0].spot_inner),		Q | S | AF,	BRTV_CONV_FLOAT_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(SPOT_OUTER_X),		F(light[0].spot_outer),		Q | S | AX,	BRTV_CONV_FIXED_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(SPOT_OUTER_F),		F(light[0].spot_outer),		Q | S | AF,	BRTV_CONV_FLOAT_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},

	{BRT(COLOUR_RGB),		F(light[0].colour),			Q | S | AF,	BRTV_CONV_COPY,				0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},

	{BRT(ATTENUATION_C_F),	F(light[0].attenuation_c),	Q | S | AF,	BRTV_CONV_FLOAT_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(ATTENUATION_C_X),	F(light[0].attenuation_c),	Q | S | AF,	BRTV_CONV_FIXED_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(ATTENUATION_L_F),	F(light[0].attenuation_l),	Q | S | AF,	BRTV_CONV_FLOAT_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(ATTENUATION_L_X),	F(light[0].attenuation_l),	Q | S | AF,	BRTV_CONV_FIXED_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(ATTENUATION_Q_F),	F(light[0].attenuation_q),	Q | S | AF,	BRTV_CONV_FLOAT_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
	{BRT(ATTENUATION_Q_X),	F(light[0].attenuation_q),	Q | S | AF,	BRTV_CONV_FIXED_SCALAR,		0,TM_PART | TM_INDEX | TM_INVALID_PS | TM_INVALID_PM},
};

static const GLSTATE_LIGHT s_Default = {
	BRT_NONE,				/* type				*/
	BRT_MODEL,				/* lighting_space	*/
	BR_VECTOR3(0,0,0),		/* position			*/
	BR_VECTOR3(0,0,0),		/* direction		*/
	BR_ANGLE_DEG(30.0),		/* spot_outer		*/
	BR_ANGLE_DEG(20.0),		/* spot_inner		*/
};

static br_tv_template_entry s_LightStates[GLSTATE_MAX_LIGHTS][BR_ASIZE(GLSTATEI_LightTemplateEntries)];

void GLSTATEI_InitLight(HGLSTATE hState)
{

	/* Create a different template list for each light. */
	for(int i = 0; i < GLSTATE_MAX_LIGHTS; ++i)
	{
		memcpy(&s_LightStates[i], GLSTATEI_LightTemplateEntries, sizeof(GLSTATEI_LightTemplateEntries));

		for(int j = 0; j < BR_ASIZE(GLSTATEI_LightTemplateEntries); ++j)
			s_LightStates[i][j].offset += sizeof(GLSTATE_LIGHT) * i;

		hState->templates.light[i] = BrTVTemplateAllocate(
			hState->resourceAnchor,
			const_cast<br_tv_template_entry*>(s_LightStates[i]),
			BR_ASIZE(s_LightStates[i])
		);
		hState->default.light[i] = s_Default;
	}

	hState->default.valid |= GLSTATE_MASK_LIGHT;
}