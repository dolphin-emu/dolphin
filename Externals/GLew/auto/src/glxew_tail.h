/* ------------------------------------------------------------------------ */

#ifdef GLEW_MX

typedef struct GLXEWContextStruct GLXEWContext;
extern GLenum glxewContextInit (GLXEWContext* ctx);
extern GLboolean glxewContextIsSupported (GLXEWContext* ctx, const char* name);

#define glxewInit() glxewContextInit(glxewGetContext())
#define glxewIsSupported(x) glxewContextIsSupported(glxewGetContext(), x)

#define GLXEW_GET_VAR(x) (*(const GLboolean*)&(glxewGetContext()->x))
#define GLXEW_GET_FUN(x) x

#else /* GLEW_MX */

#define GLXEW_GET_VAR(x) (*(const GLboolean*)&x)
#define GLXEW_GET_FUN(x) x

extern GLboolean glxewIsSupported (const char* name);

#endif /* GLEW_MX */

extern GLboolean glxewGetExtension (const char* name);

#ifdef __cplusplus
}
#endif

#endif /* __glxew_h__ */
