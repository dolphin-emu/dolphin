#ifndef IDENTIFIER_TO_CALLBACK
#define IDENTIFIER_TO_CALLBACK    

#ifdef __cplusplus
extern "C" {
#endif	

typedef struct IdentifierToCallback
{
	size_t identifier;
	void* callback;
	
} IdentifierToCallback;
	
#ifdef __cplusplus
}
#endif

#endif
