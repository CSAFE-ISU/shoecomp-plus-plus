#include "ui/surfaceRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ui/eulerAngles.h"

// GL headers only on the OpenGL backend; everything else here is
// deliberately GL-free so it stays testable and portable.
#if defined(HELLOIMGUI_HAS_OPENGL)
#include "hello_imgui/hello_imgui.h"
#include "hello_imgui/hello_imgui_include_opengl.h"
#endif

namespace shoecomp
{
    // =================================================================
    // GL-free math: camera, mesh build, grid inversion
    // =================================================================
    Eigen::Vector3f OrbitCamera::eye() const
    {
        // Turntable: fixed viewpoint straight in front of the target
        // (looking down -Z). The pitch/yaw/roll angles rotate the
        // scene, not the eye.
        return target + Eigen::Vector3f(0.0f, 0.0f, distance);
    }

    Eigen::Matrix4f OrbitCamera::view() const
    {
        // Rotate the scene about the target by R(pitch,yaw,roll) (the
        // shared convention), then translate it in front of a fixed
        // camera at distance along +Z. view = Td * R *
        // Translate(-target).
        Eigen::Matrix4f Rot = Eigen::Matrix4f::Identity();
        Rot.block<3, 3>(0, 0) = eulerToMatrixRad(pitch, yaw, roll);
        Eigen::Matrix4f Ti = Eigen::Matrix4f::Identity();
        Ti.block<3, 1>(0, 3) = -target;
        Eigen::Matrix4f Td = Eigen::Matrix4f::Identity();
        Td(2, 3) = -distance;
        return Td * Rot * Ti;
    }

    Eigen::Matrix4f OrbitCamera::proj(float aspect) const
    {
        if (aspect <= 0.0f) aspect = 1.0f;
        float f = 1.0f / std::tan(fovY * 0.5f);
        Eigen::Matrix4f P = Eigen::Matrix4f::Zero();
        P(0, 0) = f / aspect;
        P(1, 1) = f;
        P(2, 2) = (farZ + nearZ) / (nearZ - farZ);
        P(2, 3) = (2.0f * farZ * nearZ) / (nearZ - farZ);
        P(3, 2) = -1.0f;
        return P;
    }

    bool SurfaceModelParams::modelToGrid(const Eigen::Vector3f& m,
                                         int& col, int& row) const
    {
        if (sx <= 0.0 || sy <= 0.0) return false;
        double c = m.x() / sx + cx;
        double r = -m.y() / sy + cy;
        long ci = std::lround(c);
        long ri = std::lround(r);
        if (ci < 0 || ci >= sizeX || ri < 0 || ri >= sizeY)
            return false;
        col = (int)ci;
        row = (int)ri;
        return true;
    }

    SurfaceModelParams computeModelParams(const Surface3D& surf)
    {
        SurfaceModelParams P;
        const int X = surf.sizeX, Y = surf.sizeY;
        if (X <= 0 || Y <= 0) return P;

        double physW = (X > 1 ? (X - 1) : 1) * surf.incX;
        double physH = (Y > 1 ? (Y - 1) : 1) * surf.incY;
        double maxExt = std::max(physW, physH);
        if (maxExt <= 0.0) maxExt = 1.0;
        double s = 1.0 / maxExt;

        P.sizeX = X;
        P.sizeY = Y;
        P.sx = surf.incX * s;
        P.sy = surf.incY * s;
        P.cx = (X - 1) * 0.5;
        P.cy = (Y - 1) * 0.5;
        P.zMin = surf.zMin;
        float zRange = surf.zMax - surf.zMin;
        if (!(zRange > 0.0f)) zRange = 1.0f;
        P.zRange = zRange;
        P.zc = 0.5f * (surf.zMin + surf.zMax);
        // Exaggerate relief so shallow surfaces read as 3D: map the
        // full Z range to ~0.35 model units.
        P.zScale = 0.35f / zRange;
        return P;
    }

    Eigen::Vector3f physToModelDir(const SurfaceModelParams& P,
                                   const Surface3D& surf,
                                   const Eigen::Vector3d& d)
    {
        double sxr = (surf.incX != 0.0) ? P.sx / surf.incX : 0.0;
        double syr = (surf.incY != 0.0) ? P.sy / surf.incY : 0.0;
        double szr =
            (surf.incZ != 0.0) ? (double)P.zScale / surf.incZ : 0.0;
        return Eigen::Vector3f((float)(d.x() * sxr),
                               (float)(-d.y() * syr),
                               (float)(d.z() * szr));
    }

    Eigen::Affine3d physicalToModel(const Surface3D& surf)
    {
        const SurfaceModelParams P = computeModelParams(surf);
        const double kx = surf.incX != 0.0 ? P.sx / surf.incX : 0.0;
        const double ky = surf.incY != 0.0 ? P.sy / surf.incY : 0.0;
        const double kz =
            surf.incZ != 0.0 ? (double)P.zScale / surf.incZ : 0.0;
        Eigen::Affine3d A = Eigen::Affine3d::Identity();
        A.linear() = Eigen::Vector3d(kx, -ky, kz).asDiagonal();
        A.translation() = Eigen::Vector3d(
            -(surf.offX * kx + P.cx * P.sx),
            surf.offY * ky + P.cy * P.sy,
            -(surf.offZ * kz + (double)P.zc * P.zScale));
        return A;
    }

    void buildSurfaceMesh(const Surface3D& surf, SurfaceMesh& out)
    {
        const int X = surf.sizeX, Y = surf.sizeY;
        out.vertices.clear();
        out.indices.clear();
        SurfaceModelParams& P = out.params;
        P = computeModelParams(surf);
        if (X <= 0 || Y <= 0) return;

        const size_t nv = (size_t)X * Y;
        out.vertices.assign(nv * 7, 0.0f);
        std::vector<uint8_t> valid(nv, 0);

        for (int r = 0; r < Y; ++r)
        {
            for (int c = 0; c < X; ++c)
            {
                size_t vi = (size_t)r * X + c;
                float zv = surf.z(r, c);
                float x = (float)((c - P.cx) * P.sx);
                float y = (float)(-(r - P.cy) * P.sy);
                float z = 0.0f, t = 0.0f;
                if (std::isfinite(zv))
                {
                    z = (zv - P.zc) * P.zScale;
                    t = (zv - P.zMin) / P.zRange;
                    valid[vi] = 1;
                }
                float* V = &out.vertices[vi * 7];
                V[0] = x;
                V[1] = y;
                V[2] = z;
                V[3] = 0.0f;
                V[4] = 0.0f;
                V[5] = 1.0f;
                V[6] = t;
            }
        }

        auto pos = [&](int r, int c)
        {
            const float* V = &out.vertices[((size_t)r * X + c) * 7];
            return Eigen::Vector3f(V[0], V[1], V[2]);
        };
        for (int r = 0; r < Y; ++r)
        {
            for (int c = 0; c < X; ++c)
            {
                size_t vi = (size_t)r * X + c;
                if (!valid[vi]) continue;
                int cl = std::max(0, c - 1),
                    cr = std::min(X - 1, c + 1);
                int ru = std::max(0, r - 1),
                    rd = std::min(Y - 1, r + 1);
                Eigen::Vector3f Pc = pos(r, c);
                Eigen::Vector3f Pl =
                    valid[(size_t)r * X + cl] ? pos(r, cl) : Pc;
                Eigen::Vector3f Pr =
                    valid[(size_t)r * X + cr] ? pos(r, cr) : Pc;
                Eigen::Vector3f Pu =
                    valid[(size_t)ru * X + c] ? pos(ru, c) : Pc;
                Eigen::Vector3f Pd =
                    valid[(size_t)rd * X + c] ? pos(rd, c) : Pc;
                Eigen::Vector3f n = (Pr - Pl).cross(Pd - Pu);
                float len = n.norm();
                if (len > 1e-20f)
                {
                    n /= len;
                    if (n.z() < 0.0f) n = -n;
                }
                else { n = Eigen::Vector3f(0.0f, 0.0f, 1.0f); }
                float* V = &out.vertices[vi * 7];
                V[3] = n.x();
                V[4] = n.y();
                V[5] = n.z();
            }
        }

        out.indices.reserve((size_t)std::max(0, X - 1) *
                            std::max(0, Y - 1) * 6);
        for (int r = 0; r + 1 < Y; ++r)
        {
            for (int c = 0; c + 1 < X; ++c)
            {
                size_t i00 = (size_t)r * X + c;
                size_t i01 = i00 + 1;
                size_t i10 = i00 + X;
                size_t i11 = i10 + 1;
                if (valid[i00] && valid[i01] && valid[i10] &&
                    valid[i11])
                {
                    out.indices.push_back((uint32_t)i00);
                    out.indices.push_back((uint32_t)i10);
                    out.indices.push_back((uint32_t)i11);
                    out.indices.push_back((uint32_t)i00);
                    out.indices.push_back((uint32_t)i11);
                    out.indices.push_back((uint32_t)i01);
                }
            }
        }
    }

    // =================================================================
    // OpenGL implementation
    // =================================================================
#if defined(HELLOIMGUI_HAS_OPENGL)

    namespace
    {
        const char* kVert =
            "#version 330 core\n"
            "layout(location=0) in vec3 aPos;\n"
            "layout(location=1) in vec3 aNormal;\n"
            "layout(location=2) in float aT;\n"
            "uniform mat4 uMVP;\n"
            "uniform mat3 uNormalMat;\n"
            "out vec3 vNormal;\n"
            "out float vT;\n"
            "void main(){\n"
            "  gl_Position = uMVP * vec4(aPos,1.0);\n"
            "  vNormal = uNormalMat * aNormal;\n"
            "  vT = aT;\n"
            "}\n";

        const char* kFrag =
            "#version 330 core\n"
            "in vec3 vNormal;\n"
            "in float vT;\n"
            "out vec4 FragColor;\n"
            "uniform vec3 uLightDir;\n"
            "void main(){\n"
            "  vec3 base = vec3(0.902, 0.749, 0.596);\n"
            "  vec3 n = normalize(vNormal);\n"
            "  float d = abs(dot(n, normalize(uLightDir)));\n"
            "  float amb = 0.35;\n"
            "  vec3 col = base * (amb + (1.0-amb)*d);\n"
            "  FragColor = vec4(col, 1.0);\n"
            "}\n";

        GLuint compile(GLenum type, const char* src)
        {
            GLuint sh = glCreateShader(type);
            glShaderSource(sh, 1, &src, nullptr);
            glCompileShader(sh);
            GLint ok = 0;
            glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                glDeleteShader(sh);
                return 0;
            }
            return sh;
        }
    }  // namespace

    struct SurfaceRenderer::Impl
    {
        GLuint prog = 0, vao = 0, vbo = 0, ebo = 0;
        GLuint fbo = 0, colorTex = 0, depthRbo = 0;
        int fw = 0, fh = 0;
        GLsizei indexCount = 0;
        GLint uMVP = -1, uNormalMat = -1, uLightDir = -1;
        bool progTried = false, progOk = false;

        bool ensureProgram()
        {
            if (progTried) return progOk;
            progTried = true;
            GLuint vs = compile(GL_VERTEX_SHADER, kVert);
            GLuint fs = compile(GL_FRAGMENT_SHADER, kFrag);
            if (!vs || !fs)
            {
                if (vs) glDeleteShader(vs);
                if (fs) glDeleteShader(fs);
                return false;
            }
            prog = glCreateProgram();
            glAttachShader(prog, vs);
            glAttachShader(prog, fs);
            glLinkProgram(prog);
            glDeleteShader(vs);
            glDeleteShader(fs);
            GLint ok = 0;
            glGetProgramiv(prog, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                glDeleteProgram(prog);
                prog = 0;
                return false;
            }
            uMVP = glGetUniformLocation(prog, "uMVP");
            uNormalMat = glGetUniformLocation(prog, "uNormalMat");
            uLightDir = glGetUniformLocation(prog, "uLightDir");
            progOk = true;
            return true;
        }

        void ensureFBO(int w, int h)
        {
            if (fbo && fw == w && fh == h) return;
            if (!fbo) glGenFramebuffers(1, &fbo);
            if (!colorTex) glGenTextures(1, &colorTex);
            if (!depthRbo) glGenRenderbuffers(1, &depthRbo);
            glBindTexture(GL_TEXTURE_2D, colorTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR);
            glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                                  w, h);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, colorTex, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                      GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, depthRbo);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            fw = w;
            fh = h;
        }

        void release()
        {
            if (ebo) glDeleteBuffers(1, &ebo);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (vao) glDeleteVertexArrays(1, &vao);
            if (depthRbo) glDeleteRenderbuffers(1, &depthRbo);
            if (colorTex) glDeleteTextures(1, &colorTex);
            if (fbo) glDeleteFramebuffers(1, &fbo);
            if (prog) glDeleteProgram(prog);
            ebo = vbo = vao = depthRbo = colorTex = fbo = prog = 0;
            indexCount = 0;
            progTried = progOk = false;
        }

        ~Impl() { release(); }
    };

    SurfaceRenderer::SurfaceRenderer() : impl_(new Impl) {}
    SurfaceRenderer::~SurfaceRenderer() = default;
    bool SurfaceRenderer::supported() { return true; }

    void SurfaceRenderer::setSurface(const Surface3D& surf)
    {
        SurfaceMesh mesh;
        buildSurfaceMesh(surf, mesh);
        uploadMesh(mesh);
    }

    void SurfaceRenderer::uploadMesh(const SurfaceMesh& mesh)
    {
        params_ = mesh.params;
        Impl& d = *impl_;
        if (!d.vao) glGenVertexArrays(1, &d.vao);
        if (!d.vbo) glGenBuffers(1, &d.vbo);
        if (!d.ebo) glGenBuffers(1, &d.ebo);
        glBindVertexArray(d.vao);
        glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     (GLsizeiptr)(mesh.vertices.size() * sizeof(float)),
                     mesh.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            (GLsizeiptr)(mesh.indices.size() * sizeof(uint32_t)),
            mesh.indices.data(), GL_STATIC_DRAW);
        const GLsizei stride = 7 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                              (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                              (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
        d.indexCount = (GLsizei)mesh.indices.size();
    }

    uintptr_t SurfaceRenderer::render(int w, int h,
                                      const OrbitCamera& cam,
                                      const Eigen::Affine3f& modelXform)
    {
        if (w <= 0 || h <= 0) return 0;
        Impl& d = *impl_;
        if (!d.ensureProgram() || d.indexCount == 0) return 0;
        d.ensureFBO(w, h);
        lastW_ = w;
        lastH_ = h;

        GLint prevFbo = 0, prevVp[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        glGetIntegerv(GL_VIEWPORT, prevVp);

        glBindFramebuffer(GL_FRAMEBUFFER, d.fbo);
        glViewport(0, 0, w, h);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        Eigen::Matrix4f mvp = cam.proj((float)w / (float)h) *
                              cam.view() * modelXform.matrix();
        Eigen::Matrix3f nmat =
            modelXform.linear().inverse().transpose();
        Eigen::Vector3f light =
            Eigen::Vector3f(0.4f, 0.75f, 0.55f).normalized();

        glUseProgram(d.prog);
        glUniformMatrix4fv(d.uMVP, 1, GL_FALSE, mvp.data());
        glUniformMatrix3fv(d.uNormalMat, 1, GL_FALSE, nmat.data());
        glUniform3fv(d.uLightDir, 1, light.data());
        glBindVertexArray(d.vao);
        glDrawElements(GL_TRIANGLES, d.indexCount, GL_UNSIGNED_INT,
                       nullptr);
        glBindVertexArray(0);
        glUseProgram(0);

        glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
        glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);
        return (uintptr_t)d.colorTex;
    }

    bool SurfaceRenderer::pick(int px, int py, const OrbitCamera& cam,
                               const Eigen::Affine3f& modelXform,
                               int& col, int& row)
    {
        Impl& d = *impl_;
        if (!d.fbo || lastW_ <= 0 || lastH_ <= 0) return false;
        if (px < 0 || py < 0 || px >= lastW_ || py >= lastH_)
            return false;

        GLint prevFbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, d.fbo);
        float depth = 1.0f;
        // GL pixel origin is bottom-left; input py is top-left.
        glReadPixels(px, lastH_ - 1 - py, 1, 1, GL_DEPTH_COMPONENT,
                     GL_FLOAT, &depth);
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
        if (depth >= 1.0f) return false;  // background

        float ndcx = 2.0f * px / lastW_ - 1.0f;
        float ndcy = 1.0f - 2.0f * py / lastH_;
        float ndcz = 2.0f * depth - 1.0f;
        Eigen::Matrix4f mvp = cam.proj((float)lastW_ / (float)lastH_) *
                              cam.view() * modelXform.matrix();
        Eigen::Vector4f clip(ndcx, ndcy, ndcz, 1.0f);
        Eigen::Vector4f p = mvp.inverse() * clip;
        if (p.w() == 0.0f) return false;
        Eigen::Vector3f model = p.head<3>() / p.w();
        return params_.modelToGrid(model, col, row);
    }

    bool SurfaceRenderer::readPixelsRGBA(
        std::vector<unsigned char>& out, int& w, int& h)
    {
        Impl& d = *impl_;
        if (!d.fbo || lastW_ <= 0 || lastH_ <= 0) return false;
        w = lastW_;
        h = lastH_;
        out.assign((size_t)w * h * 4, 0);
        GLint prevFbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, d.fbo);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
        // GL origin is bottom-left; flip rows to top-down.
        const size_t rowBytes = (size_t)w * 4;
        std::vector<unsigned char> tmp(rowBytes);
        for (int y = 0; y < h / 2; ++y)
        {
            unsigned char* a = out.data() + (size_t)y * rowBytes;
            unsigned char* b =
                out.data() + (size_t)(h - 1 - y) * rowBytes;
            std::copy(a, a + rowBytes, tmp.data());
            std::copy(b, b + rowBytes, a);
            std::copy(tmp.data(), tmp.data() + rowBytes, b);
        }
        return true;
    }

    void SurfaceRenderer::releaseGL()
    {
        if (impl_) impl_->release();
    }

#else  // !HELLOIMGUI_HAS_OPENGL

    struct SurfaceRenderer::Impl
    {
    };
    SurfaceRenderer::SurfaceRenderer() = default;
    SurfaceRenderer::~SurfaceRenderer() = default;
    bool SurfaceRenderer::supported() { return false; }
    void SurfaceRenderer::setSurface(const Surface3D&) {}
    void SurfaceRenderer::uploadMesh(const SurfaceMesh&) {}
    uintptr_t SurfaceRenderer::render(int, int, const OrbitCamera&,
                                      const Eigen::Affine3f&)
    {
        return 0;
    }
    bool SurfaceRenderer::pick(int, int, const OrbitCamera&,
                               const Eigen::Affine3f&, int&, int&)
    {
        return false;
    }
    bool SurfaceRenderer::readPixelsRGBA(std::vector<unsigned char>&,
                                         int&, int&)
    {
        return false;
    }
    void SurfaceRenderer::releaseGL() {}

#endif
}  // namespace shoecomp
