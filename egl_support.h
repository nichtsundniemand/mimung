#ifndef EGL_SUPPORT_H
	#define EGL_SUPPORT_H

void egl_debug_handler(
	EGLenum error, const char* cmd, EGLint msg_type,
	EGLLabelKHR thr_lbl, EGLLabelKHR obj_lbl, const char* msg
) {
	char* type;
	switch(msg_type) {
		case EGL_DEBUG_MSG_INFO_KHR:
    			type = "INFO";
    			break;
		case EGL_DEBUG_MSG_WARN_KHR:
    			type = "WARN";
    			break;
		case EGL_DEBUG_MSG_ERROR_KHR:
    			type = "ERROR";
    			break;
		case EGL_DEBUG_MSG_CRITICAL_KHR:
    			type = "CRITICAL";
    			break;
	}
	fprintf(
    		stderr,
    		"Debug handler (%s):\n"
    		"    - cmd: %s\n"
    		"    - msg: %s\n",
    		type, cmd, msg
	);
}

static PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR;
static PFNEGLQUERYDEBUGKHRPROC eglQueryDebugKHR;

int egl_init_extensions() {
	printf("egl_init_extensions():\n");
	if((eglDebugMessageControlKHR =	
		(PFNEGLDEBUGMESSAGECONTROLKHRPROC)eglGetProcAddress("eglDebugMessageControlKHR")
		) == NULL ||
	   (eglQueryDebugKHR =	
		(PFNEGLQUERYDEBUGKHRPROC)eglGetProcAddress("eglQueryDebugKHR")
		) == NULL
	) {
		printf("    Error: Could not get extension address!\n");
		return 1;
	}
	
	printf(
		"    Success:\n"
		"      - eglDebugMessageControlKHR = %p\n"
		"      - eglQueryDebugKHR = %p\n",
		eglDebugMessageControlKHR,
		eglQueryDebugKHR
	);
	return 0;
}

#endif
