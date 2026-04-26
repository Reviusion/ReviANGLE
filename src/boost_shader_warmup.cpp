// Boost: Shader Warmup
//
// PROBLEM (old version):
//   The original implementation called glDrawArrays(GL_TRIANGLES, 0, 3) with
//   no vertex array bound. ANGLE's GLES validation rejects this with
//   GL_INVALID_OPERATION before the shader is exercised - so warmup did
//   nothing. (See angle_log.txt: "shader_warmup: warmed up 0 programs".)
//
// FIX:
//   ANGLE's D3D11 backend compiles HLSL during glLinkProgram, but defers some
//   driver-variant compilation (depth-only, color-mask permutations) until
//   first use. glValidateProgram forces the driver to resolve uniforms and
//   verify the program against the current vertex layout - which on D3D11
//   triggers any deferred HLSL JIT.
//
//   We iterate program IDs 1..255 (ANGLE assigns small IDs in order), check
//   each via glIsProgram, then validate. No drawing required - no risk of
//   GL_INVALID_OPERATION pollution.
//
//   Drained by glGetError after each call to clear any spurious validation
//   errors (e.g. "vertex layout mismatch" - we only care about the JIT
//   side-effect, not the validation result).

#include <windows.h>
#include "config.hpp"
#include "gl_proxy.hpp"
#include "angle_loader.hpp"

typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef unsigned char GLboolean;

namespace boost_shader_warmup {

    void apply() {
        if (!Config::get().shader_warmup) return;

        using IsProgramFn  = GLboolean(WINAPI*)(GLuint);
        using ValidateFn   = void(WINAPI*)(GLuint);
        using GetErrorFn   = GLenum(WINAPI*)(void);

        auto isProgram = (IsProgramFn) glproxy::resolve("glIsProgram");
        auto validate  = (ValidateFn)  glproxy::resolve("glValidateProgram");
        auto getError  = (GetErrorFn)  glproxy::resolve("glGetError");

        if (!isProgram || !validate) {
            angle::log("shader_warmup: glIsProgram / glValidateProgram unavailable");
            return;
        }

        // Drain any error state from prior init (apply() runs after GL is up
        // but cocos2d may have left a benign error in the queue).
        if (getError) while (getError() != 0) {}

        int validated = 0;
        for (GLuint prog = 1; prog <= 255; prog++) {
            if (isProgram(prog)) {
                validate(prog);
                if (getError) getError();  // drain validation status
                validated++;
            }
        }

        angle::log("shader_warmup: validated %d linked programs (forces D3D11 HLSL JIT)", validated);
    }
}
