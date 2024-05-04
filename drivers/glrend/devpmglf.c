/*
 * Device pixelmap implementation, front/screen edition.
 */

#include "drv.h"
#include <brassert.h>

/*
 * Default dispatch table for device (defined at end of file)
 */
static const struct br_device_pixelmap_dispatch devicePixelmapFrontDispatch;

static br_error custom_query(br_value *pvalue, void **extra, br_size_t *pextra_size, void *block,
                             const struct br_tv_template_entry *tep)
{
    const br_device_pixelmap *self = block;

    switch(tep->token) {
        case BRT_OPENGL_EXT_PROCS_P:
            pvalue->p = (void *)&self->asFront.ext_procs;
            break;
        default:
            return BRE_UNKNOWN;
    }

    return BRE_OK;
}

static const br_tv_custom custom = {
    .query      = custom_query,
    .set        = NULL,
    .extra_size = NULL,
};

/*
 * Device pixelmap info. template
 */
#define F(f)  offsetof(br_device_pixelmap, f)
#define FF(f) offsetof(br_device_pixelmap, asFront.f)
static struct br_tv_template_entry devicePixelmapFrontTemplateEntries[] = {
    {BRT(WIDTH_I32),            F(pm_width),        BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U16, 0                    },
    {BRT(HEIGHT_I32),           F(pm_height),       BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U16, 0                    },
    {BRT(PIXEL_TYPE_U8),        F(pm_type),         BRTV_QUERY | BRTV_ALL, BRTV_CONV_I32_U8,  0                    },
    {BRT(OUTPUT_FACILITY_O),    F(output_facility), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY,    0                    },
    {BRT(FACILITY_O),           F(output_facility), BRTV_QUERY,            BRTV_CONV_COPY,    0                    },
    {BRT(IDENTIFIER_CSTR),      F(pm_identifier),   BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY,    0                    },
    {BRT(MSAA_SAMPLES_I32),     F(msaa_samples),    BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY,    0                    },
    {BRT(OPENGL_EXT_PROCS_P),   0,                  BRTV_QUERY | BRTV_ALL, BRTV_CONV_CUSTOM,  (br_uintptr_t)&custom},
    {BRT(OPENGL_VERSION_CSTR),  FF(gl_version),     BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY,    0                    },
    {BRT(OPENGL_VENDOR_CSTR),   FF(gl_vendor),      BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY,    0                    },
    {BRT(OPENGL_RENDERER_CSTR), FF(gl_renderer),    BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY,    0                    },

    {DEV(OPENGL_NUM_EXTENSIONS_I32), FF(gl_num_extensions), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY,    0                    },
    {DEV(OPENGL_EXTENSIONS_PL),      FF(gl_extensions),     BRTV_QUERY | BRTV_ALL, BRTV_CONV_LIST,    0                    },
};
#undef FF
#undef F

struct pixelmapNewTokens {
    br_int_32               width;
    br_int_32               height;
    br_int_32               pixel_bits;
    br_uint_8               pixel_type;
    int                     msaa_samples;
    br_device_gl_ext_procs *ext_procs;
    const char             *vertex_shader;
    const char             *fragment_shader;
};

#define F(f) offsetof(struct pixelmapNewTokens, f)
static struct br_tv_template_entry pixelmapNewTemplateEntries[] = {
    {BRT_WIDTH_I32,                  NULL, F(width),           BRTV_SET, BRTV_CONV_COPY},
    {BRT_HEIGHT_I32,                 NULL, F(height),          BRTV_SET, BRTV_CONV_COPY},
    {BRT_PIXEL_BITS_I32,             NULL, F(pixel_bits),      BRTV_SET, BRTV_CONV_COPY},
    {BRT_PIXEL_TYPE_U8,              NULL, F(pixel_type),      BRTV_SET, BRTV_CONV_COPY},
    {BRT_MSAA_SAMPLES_I32,           NULL, F(msaa_samples),    BRTV_SET, BRTV_CONV_COPY},
    {BRT_OPENGL_EXT_PROCS_P,         NULL, F(ext_procs),       BRTV_SET, BRTV_CONV_COPY},
    {BRT_OPENGL_VERTEX_SHADER_STR,   NULL, F(vertex_shader),   BRTV_SET, BRTV_CONV_COPY},
    {BRT_OPENGL_FRAGMENT_SHADER_STR, NULL, F(fragment_shader), BRTV_SET, BRTV_CONV_COPY},
};
#undef F

br_device_pixelmap *DevicePixelmapGLAllocateFront(br_device *dev, br_output_facility *outfcty, br_token_value *tv)
{
    br_device_pixelmap      *self;
    br_int_32                count;
    GLint                    red_bits = 0, grn_bits = 0, blu_bits = 0, alpha_bits = 0;
    struct pixelmapNewTokens pt = {
        .width           = -1,
        .height          = -1,
        .pixel_bits      = -1,
        .pixel_type      = BR_PMT_MAX,
        .msaa_samples    = 0,
        .ext_procs       = NULL,
        .vertex_shader   = NULL,
        .fragment_shader = NULL,
    };

    if(dev->templates.pixelmapNewTemplate == NULL) {
        dev->templates.pixelmapNewTemplate = BrTVTemplateAllocate(dev, pixelmapNewTemplateEntries,
                                                                  BR_ASIZE(pixelmapNewTemplateEntries));
    }

    BrTokenValueSetMany(&pt, &count, NULL, tv, dev->templates.pixelmapNewTemplate);

    if(pt.ext_procs == NULL || pt.width <= 0 || pt.height <= 0)
        return NULL;

    if((pt.pixel_type = DeviceGLTypeOrBits(pt.pixel_type, pt.pixel_bits)) == BR_PMT_MAX)
        return NULL;

    self                  = BrResAllocate(dev->res, sizeof(br_device_pixelmap), BR_MEMORY_OBJECT);
    self->pm_identifier   = BrResSprintf(self, "OpenGL:Screen:%dx%d", pt.width, pt.height);
    self->dispatch        = &devicePixelmapFrontDispatch;
    self->device          = dev;
    self->output_facility = outfcty;
    self->use_type        = BRT_NONE;
    self->msaa_samples    = pt.msaa_samples;
    self->screen          = self;

    self->pm_type   = pt.pixel_type;
    self->pm_width  = pt.width;
    self->pm_height = pt.height;
    self->pm_flags |= BR_PMF_NO_ACCESS;

    /*
     * Make a copy, so they can't switch things out from under us.
     */
    self->asFront.ext_procs = *pt.ext_procs;

    if((self->asFront.gl_context = DevicePixelmapGLExtCreateContext(self)) == NULL) {
        BrResFreeNoCallback(self);
        return NULL;
    }

    if(DevicePixelmapGLExtMakeCurrent(self, self->asFront.gl_context) != BRE_OK)
        goto cleanup_context;

    if(gladLoadGLLoader(DevicePixelmapGLExtGetGetProcAddress(self)) == 0) {
        BrLogError("GLREND", "Unable to load OpenGL functions.");
        goto cleanup_context;
    }

    self->asFront.gl_version  = BrResStrDup(self, (const char *)glGetString(GL_VERSION));
    self->asFront.gl_vendor   = BrResStrDup(self, (const char *)glGetString(GL_VENDOR));
    self->asFront.gl_renderer = BrResStrDup(self, (const char *)glGetString(GL_RENDERER));

    BrLogTrace("GLREND", "OpenGL Version  = %s", self->asFront.gl_version);
    BrLogTrace("GLREND", "OpenGL Vendor   = %s", self->asFront.gl_vendor);
    BrLogTrace("GLREND", "OpenGL Renderer = %s", self->asFront.gl_renderer);

    /*
     * Get a copy of the extension list.
     * NULL-terminate so we can expose it as a BRT_POINTER_LIST.
     */
    glGetIntegerv(GL_NUM_EXTENSIONS, &self->asFront.gl_num_extensions);

    self->asFront.gl_extensions = BrResAllocate(self, sizeof(char *) * (self->asFront.gl_num_extensions + 1), BR_MEMORY_DRIVER);
    for(GLuint i = 0; i < self->asFront.gl_num_extensions; ++i) {
        const GLubyte *ext             = glGetStringi(GL_EXTENSIONS, i);
        self->asFront.gl_extensions[i] = BrResStrDup(self->asFront.gl_extensions, (const char *)ext);
    }
    self->asFront.gl_extensions[self->asFront.gl_num_extensions] = NULL;

    /*
     * Try to figure out the actual format we got.
     * This isn't a big deal if we don't know what it is - we can only be written to by a doubleBuffer().
     */
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &red_bits);
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &grn_bits);
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &blu_bits);
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &alpha_bits);

    if(red_bits == 5 && grn_bits == 6 && blu_bits == 5) {
        self->pm_type = BR_PMT_RGB_565;
    } else if(red_bits == 8 && grn_bits == 8 && blu_bits == 8 && alpha_bits == 0) {
        self->pm_type = BR_PMT_RGBX_888;
    } else if(red_bits == 8 && grn_bits == 8 && blu_bits == 8 && alpha_bits == 8) {
        self->pm_type = BR_PMT_RGBA_8888;
    } else {
        BrLogWarn("GLREND", "OpenGL gave us an unknown screen format (R%dG%dB%dA%d), soldiering on...", red_bits,
                  grn_bits, blu_bits, alpha_bits);
    }

    if(VIDEO_Open(&self->asFront.video, pt.vertex_shader, pt.fragment_shader) == NULL)
        goto cleanup_context;

    self->asFront.tex_white        = DeviceGLBuildWhiteTexture();
    self->asFront.tex_checkerboard = DeviceGLBuildCheckerboardTexture();

    /*
     * We can't use BRender's fonts directly, so build a POT texture with
     * glyph from left-to-right. All fonts have 256 possible characters.
     */

    BrLogTrace("GLREND", "Building fixed 3x5 font atlas.");
    (void)FontGLBuildAtlas(&self->asFront.font_fixed3x5, BrFontFixed3x5, 128, 64);

    BrLogTrace("GLREND", "Building proportional 4x6 font atlas.");
    (void)FontGLBuildAtlas(&self->asFront.font_prop4x6, BrFontProp4x6, 128, 64);

    BrLogTrace("GLREND", "Building proportional 7x9 font atlas.");
    (void)FontGLBuildAtlas(&self->asFront.font_prop7x9, BrFontProp7x9, 256, 64);

    self->asFront.num_refs = 0;

    ObjectContainerAddFront(self->output_facility, (br_object *)self);
    return self;

cleanup_context:
    DevicePixelmapGLExtDeleteContext(self, self->asFront.gl_context);
    BrResFree(self);
    return NULL;
}

static void BR_CMETHOD_DECL(br_device_pixelmap_glf, free)(br_object *_self)
{
    br_device_pixelmap *self = (br_device_pixelmap *)_self;

    BrLogTrace("GLREND", "Freeing %s", self->pm_identifier);

    UASSERT(self->asFront.num_refs == 0);

    glDeleteTextures(1, &self->asFront.font_prop7x9.tex);
    glDeleteTextures(1, &self->asFront.font_prop4x6.tex);
    glDeleteTextures(1, &self->asFront.font_fixed3x5.tex);
    glDeleteTextures(1, &self->asFront.tex_checkerboard);
    glDeleteTextures(1, &self->asFront.tex_white);

    VIDEO_Close(&self->asFront.video);

    DevicePixelmapGLExtDeleteContext(self, self->asFront.gl_context);

    ObjectContainerRemove(self->output_facility, (br_object *)self);

    DevicePixelmapGLExtFree(self);

    BrResFreeNoCallback(self);
}

const char *BR_CMETHOD_DECL(br_device_pixelmap_glf, identifier)(br_object *self)
{
    return ((br_device_pixelmap *)self)->pm_identifier;
}

br_token BR_CMETHOD_DECL(br_device_pixelmap_glf, type)(br_object *self)
{
    (void)self;
    return BRT_DEVICE_PIXELMAP;
}

br_boolean BR_CMETHOD_DECL(br_device_pixelmap_glf, isType)(br_object *self, br_token t)
{
    (void)self;
    return (t == BRT_DEVICE_PIXELMAP) || (t == BRT_OBJECT);
}

br_device *BR_CMETHOD_DECL(br_device_pixelmap_glf, device)(br_object *self)
{
    (void)self;
    return ((br_device_pixelmap *)self)->device;
}

br_size_t BR_CMETHOD_DECL(br_device_pixelmap_glf, space)(br_object *self)
{
    (void)self;
    return sizeof(br_device_pixelmap);
}

struct br_tv_template *BR_CMETHOD_DECL(br_device_pixelmap_glf, templateQuery)(br_object *_self)
{
    br_device_pixelmap *self = (br_device_pixelmap *)_self;

    if(self->device->templates.devicePixelmapFrontTemplate == NULL)
        self->device->templates.devicePixelmapFrontTemplate = BrTVTemplateAllocate(
            self->device, devicePixelmapFrontTemplateEntries, BR_ASIZE(devicePixelmapFrontTemplateEntries));

    return self->device->templates.devicePixelmapFrontTemplate;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_glf, resize)(br_device_pixelmap *self, br_int_32 width, br_int_32 height)
{
    br_error err;

    if((err = DevicePixelmapGLExtResize(self, width, height)) != BRE_OK)
        return err;

    /*
     * Resizing, assumed to have been done externally
     * via whatever window we're representing.
     */
    self->pm_width  = width;
    self->pm_height = height;
    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_glf, doubleBuffer)(br_device_pixelmap *self, br_device_pixelmap *src)
{
    /*
     * Ignore self-blit.
     */
    if(self == src)
        return BRE_OK;

    if(ObjectDevice(src) != self->device)
        return BRE_UNSUPPORTED;

    if(self->use_type != BRT_NONE || src->use_type != BRT_OFFSCREEN)
        return BRE_UNSUPPORTED;

    /*
     * Blit.
     */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src->asBack.glFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    glBlitFramebuffer(0, 0, src->pm_width, src->pm_height, 0, 0, self->pm_width, self->pm_height, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    /*
     * Call our pre-swap hook
     */
    DevicePixelmapGLExtPreSwap(self, src->asBack.glFbo);

    /*
     * Drain any GL errors.
     */
    while(glGetError() != GL_NO_ERROR)
        ;

    /*
     * Finally, swap the buffers.
     */
    DevicePixelmapGLExtSwapBuffers(self);

    /*
     * Drain any GL errors (again).
     */
    while(glGetError() != GL_NO_ERROR)
        ;

    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_device_pixelmap_glf, handleWindowEvent)(br_device_pixelmap *self, void *arg)
{
    return DevicePixelmapGLExtHandleWindowEvent(self, arg);
}

/*
 * Default dispatch table for device pixelmap
 */
static const struct br_device_pixelmap_dispatch devicePixelmapFrontDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free       = BR_CMETHOD_REF(br_device_pixelmap_glf, free),
    ._identifier = BR_CMETHOD_REF(br_device_pixelmap_glf, identifier),
    ._type       = BR_CMETHOD_REF(br_device_pixelmap_glf, type),
    ._isType     = BR_CMETHOD_REF(br_device_pixelmap_glf, isType),
    ._device     = BR_CMETHOD_REF(br_device_pixelmap_glf, device),
    ._space      = BR_CMETHOD_REF(br_device_pixelmap_glf, space),

    ._templateQuery = BR_CMETHOD_REF(br_device_pixelmap_glf, templateQuery),
    ._query         = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer   = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany     = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll      = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize  = BR_CMETHOD_REF(br_object, queryAllSize),

    ._validSource = BR_CMETHOD_REF(br_device_pixelmap_null, validSource),
    ._resize      = BR_CMETHOD_REF(br_device_pixelmap_glf, resize),
    ._match       = BR_CMETHOD_REF(br_device_pixelmap_gl, match),
    ._allocateSub = BR_CMETHOD_REF(br_device_pixelmap_fail, allocateSub),

    ._copy         = BR_CMETHOD_REF(br_device_pixelmap_fail, copy),
    ._copyTo       = BR_CMETHOD_REF(br_device_pixelmap_fail, copyTo),
    ._copyFrom     = BR_CMETHOD_REF(br_device_pixelmap_fail, copyFrom),
    ._fill         = BR_CMETHOD_REF(br_device_pixelmap_fail, fill),
    ._doubleBuffer = BR_CMETHOD_REF(br_device_pixelmap_glf, doubleBuffer),

    ._copyDirty         = BR_CMETHOD_REF(br_device_pixelmap_fail, copyDirty),
    ._copyToDirty       = BR_CMETHOD_REF(br_device_pixelmap_fail, copyToDirty),
    ._copyFromDirty     = BR_CMETHOD_REF(br_device_pixelmap_fail, copyFromDirty),
    ._fillDirty         = BR_CMETHOD_REF(br_device_pixelmap_fail, fillDirty),
    ._doubleBufferDirty = BR_CMETHOD_REF(br_device_pixelmap_fail, doubleBufferDirty),

    ._rectangle                = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangle),
    ._rectangle2               = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangle2),
    ._rectangleCopy            = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleCopyTo),
    ._rectangleCopyTo          = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleCopyTo),
    ._rectangleCopyFrom        = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleCopyFrom),
    ._rectangleStretchCopy     = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyTo),
    ._rectangleStretchCopyTo   = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyTo),
    ._rectangleStretchCopyFrom = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleStretchCopyFrom),
    ._rectangleFill            = BR_CMETHOD_REF(br_device_pixelmap_fail, rectangleFill),
    ._pixelSet                 = BR_CMETHOD_REF(br_device_pixelmap_fail, pixelSet),
    ._line                     = BR_CMETHOD_REF(br_device_pixelmap_fail, line),
    ._copyBits                 = BR_CMETHOD_REF(br_device_pixelmap_fail, copyBits),

    ._text       = BR_CMETHOD_REF(br_device_pixelmap_fail, text),
    ._textBounds = BR_CMETHOD_REF(br_device_pixelmap_gen, textBounds),

    ._rowSize  = BR_CMETHOD_REF(br_device_pixelmap_fail, rowSize),
    ._rowQuery = BR_CMETHOD_REF(br_device_pixelmap_fail, rowQuery),
    ._rowSet   = BR_CMETHOD_REF(br_device_pixelmap_fail, rowSet),

    ._pixelQuery        = BR_CMETHOD_REF(br_device_pixelmap_fail, pixelQuery),
    ._pixelAddressQuery = BR_CMETHOD_REF(br_device_pixelmap_fail, pixelAddressQuery),

    ._pixelAddressSet = BR_CMETHOD_REF(br_device_pixelmap_fail, pixelAddressSet),
    ._originSet       = BR_CMETHOD_REF(br_device_pixelmap_gen, originSet),

    ._flush        = BR_CMETHOD_REF(br_device_pixelmap_fail, flush),
    ._synchronise  = BR_CMETHOD_REF(br_device_pixelmap_fail, synchronise),
    ._directLock   = BR_CMETHOD_REF(br_device_pixelmap_fail, directLock),
    ._directUnlock = BR_CMETHOD_REF(br_device_pixelmap_fail, directUnlock),
    ._getControls  = BR_CMETHOD_REF(br_device_pixelmap_fail, getControls),
    ._setControls  = BR_CMETHOD_REF(br_device_pixelmap_fail, setControls),

    ._handleWindowEvent = BR_CMETHOD_REF(br_device_pixelmap_glf, handleWindowEvent),
};