#include <ais_sim/wave.hpp>
#include <ctime>
#include <glm/gtc/constants.hpp>

namespace ais_sim {

    static glm::vec2 kVecAt(int n, int m, int N, int M, float L_x, float L_z) {
        return glm::vec2(
            2.0f * glm::pi<float>() * (n - N / 2) / L_x,
            2.0f * glm::pi<float>() * (m - M / 2) / L_z
        );
    }

    Wave::Wave(int N, int M, float L_x, float L_z, glm::vec2 omega, float V, float A, float lambda)
        : N(N), M(M), omega_hat(glm::normalize(omega)), V(V), L_x(L_x), L_z(L_z), A(A), lambda(lambda)
    {
        generator.seed(static_cast<unsigned int>(time(nullptr)));
        kNum = N * M;

        int renderCount = (N + 1) * (M + 1);
        heightField = new glm::vec3[renderCount];
        normalField = new glm::vec3[renderCount];

        value_h_twiddle_0 = new std::complex<float>[kNum];
        value_h_twiddle_0_conj = new std::complex<float>[kNum];
        value_h_twiddle = new std::complex<float>[kNum];

        slope_x_term = new std::complex<float>[kNum];
        slope_z_term = new std::complex<float>[kNum];
        D_x_term = new std::complex<float>[kNum];
        D_z_term = new std::complex<float>[kNum];

        out_height = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * kNum);
        out_slope_x = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * kNum);
        out_slope_z = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * kNum);
        out_D_x = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * kNum);
        out_D_z = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * kNum);

        kVecCache = new glm::vec2[kNum];
        omegaCache = new float[kNum];

        for (int n = 0; n < N; ++n)
            for (int m = 0; m < M; ++m) {
                int index = m * N + n;
                glm::vec2 k = kVecAt(n, m, N, M, L_x, L_z);
                kVecCache[index] = k;
                omegaCache[index] = func_omega(glm::length(k));
                std::complex<float> h0 = func_h_twiddle_0(k);
                value_h_twiddle_0[index] = h0;
                value_h_twiddle_0_conj[index] = std::conj(h0);
            }

        // built ONCE — this used to run every buildField() call, which was the perf hog
        p_height = fftwf_plan_dft_2d(N, M, (fftwf_complex*)value_h_twiddle, out_height, FFTW_BACKWARD, FFTW_ESTIMATE);
        p_slope_x = fftwf_plan_dft_2d(N, M, (fftwf_complex*)slope_x_term, out_slope_x, FFTW_BACKWARD, FFTW_ESTIMATE);
        p_slope_z = fftwf_plan_dft_2d(N, M, (fftwf_complex*)slope_z_term, out_slope_z, FFTW_BACKWARD, FFTW_ESTIMATE);
        p_D_x = fftwf_plan_dft_2d(N, M, (fftwf_complex*)D_x_term, out_D_x, FFTW_BACKWARD, FFTW_ESTIMATE);
        p_D_z = fftwf_plan_dft_2d(N, M, (fftwf_complex*)D_z_term, out_D_z, FFTW_BACKWARD, FFTW_ESTIMATE);
    }

    Wave::~Wave() {
        if (p_height) fftwf_destroy_plan(p_height);
        if (p_slope_x) fftwf_destroy_plan(p_slope_x);
        if (p_slope_z) fftwf_destroy_plan(p_slope_z);
        if (p_D_x) fftwf_destroy_plan(p_D_x);
        if (p_D_z) fftwf_destroy_plan(p_D_z);

        fftwf_free(out_height);
        fftwf_free(out_slope_x);
        fftwf_free(out_slope_z);
        fftwf_free(out_D_x);
        fftwf_free(out_D_z);

        delete[] slope_x_term;
        delete[] slope_z_term;
        delete[] D_x_term;
        delete[] D_z_term;

        delete[] heightField;
        delete[] normalField;
        delete[] value_h_twiddle_0;
        delete[] value_h_twiddle;
        delete[] value_h_twiddle_0_conj;
        delete[] kVecCache;
        delete[] omegaCache;
    }

    Wave::Wave(Wave&& other) noexcept
        : heightField(other.heightField),
        normalField(other.normalField),
        N(other.N), M(other.M),
        value_h_twiddle_0(other.value_h_twiddle_0),
        value_h_twiddle_0_conj(other.value_h_twiddle_0_conj),
        value_h_twiddle(other.value_h_twiddle),
        slope_x_term(other.slope_x_term),
        slope_z_term(other.slope_z_term),
        D_x_term(other.D_x_term),
        D_z_term(other.D_z_term),
        out_height(other.out_height),
        out_slope_x(other.out_slope_x),
        out_slope_z(other.out_slope_z),
        out_D_x(other.out_D_x),
        out_D_z(other.out_D_z),
        p_height(other.p_height),
        p_slope_x(other.p_slope_x),
        p_slope_z(other.p_slope_z),
        p_D_x(other.p_D_x),
        p_D_z(other.p_D_z),
        kVecCache(other.kVecCache),
        omegaCache(other.omegaCache),
        generator(std::move(other.generator)),
        normal_dist(std::move(other.normal_dist)),
        A(other.A), V(other.V), lambda(other.lambda),
        kNum(other.kNum),
        omega_hat(other.omega_hat),
        L_x(other.L_x), L_z(other.L_z)
    {
        other.heightField = nullptr;
        other.normalField = nullptr;
        other.value_h_twiddle_0 = nullptr;
        other.value_h_twiddle_0_conj = nullptr;
        other.value_h_twiddle = nullptr;
        other.slope_x_term = nullptr;
        other.slope_z_term = nullptr;
        other.D_x_term = nullptr;
        other.D_z_term = nullptr;
        other.out_height = nullptr;
        other.out_slope_x = nullptr;
        other.out_slope_z = nullptr;
        other.out_D_x = nullptr;
        other.out_D_z = nullptr;
        other.p_height = nullptr;
        other.p_slope_x = nullptr;
        other.p_slope_z = nullptr;
        other.p_D_x = nullptr;
        other.p_D_z = nullptr;
        other.kVecCache = nullptr;
        other.omegaCache = nullptr;
    }


    Wave& Wave::operator=(Wave&& other) noexcept {
        if (this != &other) {
            if (p_height) fftwf_destroy_plan(p_height);
            if (p_slope_x) fftwf_destroy_plan(p_slope_x);
            if (p_slope_z) fftwf_destroy_plan(p_slope_z);
            if (p_D_x) fftwf_destroy_plan(p_D_x);
            if (p_D_z) fftwf_destroy_plan(p_D_z);
            fftwf_free(out_height);
            fftwf_free(out_slope_x);
            fftwf_free(out_slope_z);
            fftwf_free(out_D_x);
            fftwf_free(out_D_z);
            delete[] slope_x_term;
            delete[] slope_z_term;
            delete[] D_x_term;
            delete[] D_z_term;
            delete[] heightField;
            delete[] normalField;
            delete[] value_h_twiddle_0;
            delete[] value_h_twiddle;
            delete[] value_h_twiddle_0_conj;
            delete[] kVecCache;
            delete[] omegaCache;

            heightField = other.heightField;
            normalField = other.normalField;
            N = other.N; M = other.M;
            value_h_twiddle_0 = other.value_h_twiddle_0;
            value_h_twiddle_0_conj = other.value_h_twiddle_0_conj;
            value_h_twiddle = other.value_h_twiddle;
            slope_x_term = other.slope_x_term;
            slope_z_term = other.slope_z_term;
            D_x_term = other.D_x_term;
            D_z_term = other.D_z_term;
            out_height = other.out_height;
            out_slope_x = other.out_slope_x;
            out_slope_z = other.out_slope_z;
            out_D_x = other.out_D_x;
            out_D_z = other.out_D_z;
            p_height = other.p_height;
            p_slope_x = other.p_slope_x;
            p_slope_z = other.p_slope_z;
            p_D_x = other.p_D_x;
            p_D_z = other.p_D_z;
            kVecCache = other.kVecCache;
            omegaCache = other.omegaCache;
            generator = std::move(other.generator);
            normal_dist = std::move(other.normal_dist);
            A = other.A; V = other.V; lambda = other.lambda;
            kNum = other.kNum;
            omega_hat = other.omega_hat;
            L_x = other.L_x; L_z = other.L_z;

            other.heightField = nullptr;
            other.normalField = nullptr;
            other.value_h_twiddle_0 = nullptr;
            other.value_h_twiddle_0_conj = nullptr;
            other.value_h_twiddle = nullptr;
            other.slope_x_term = nullptr;
            other.slope_z_term = nullptr;
            other.D_x_term = nullptr;
            other.D_z_term = nullptr;
            other.out_height = nullptr;
            other.out_slope_x = nullptr;
            other.out_slope_z = nullptr;
            other.out_D_x = nullptr;
            other.out_D_z = nullptr;
            other.p_height = nullptr;
            other.p_slope_x = nullptr;
            other.p_slope_z = nullptr;
            other.p_D_x = nullptr;
            other.p_D_z = nullptr;
            other.kVecCache = nullptr;
            other.omegaCache = nullptr;
        }
        return *this;
    }
    float Wave::func_omega(float k) const { return std::sqrt(g * k); }

    float Wave::func_P_h(glm::vec2 vec_k) const {
        if (vec_k == glm::vec2(0.0f)) return 0.0f;
        float L = V * V / g;
        float k = glm::length(vec_k);
        glm::vec2 k_hat = glm::normalize(vec_k);
        float dotVal = glm::dot(k_hat, omega_hat);
        float result = A * std::exp(-1.0f / (k * L * k * L)) / std::pow(k, 4) * std::pow(dotVal, 2);
        result *= std::exp(-k * k * l * l);
        return result;
    }

    std::complex<float> Wave::func_h_twiddle_0(glm::vec2 vec_k) {
        float xi_r = normal_dist(generator);
        float xi_i = normal_dist(generator);
        return std::sqrt(0.5f) * std::complex<float>(xi_r, xi_i) * std::sqrt(func_P_h(vec_k));
    }

    std::complex<float> Wave::func_h_twiddle(int kn, int km, float t) const {
        int index = km * N + kn;
        float k = glm::length(kVecAt(kn, km, N, M, L_x, L_z));
        auto term1 = value_h_twiddle_0[index] * std::exp(std::complex<float>(0.0f, func_omega(k) * t));
        auto term2 = value_h_twiddle_0_conj[index] * std::exp(std::complex<float>(0.0f, -func_omega(k) * t));
        return term1 + term2;
    }

    void Wave::buildField(float time) {
        for (int n = 0; n < N; n++)
            for (int m = 0; m < M; m++) {
                int index = m * N + n;
                glm::vec2 kVec = kVecCache[index];
                float omega = omegaCache[index];
                float theta = omega * time;
                float c = std::cos(theta);
                float s = std::sin(theta);

                std::complex<float> term1 = value_h_twiddle_0[index] * std::complex<float>(c, s);
                std::complex<float> term2 = value_h_twiddle_0_conj[index] * std::complex<float>(c, -s);
                value_h_twiddle[index] = term1 + term2;
                float kLength = glm::length(kVec);
                glm::vec2 kVecNorm = kLength == 0 ? kVec : glm::normalize(kVec);

                slope_x_term[index] = std::complex<float>(0, kVec.x) * value_h_twiddle[index];
                slope_z_term[index] = std::complex<float>(0, kVec.y) * value_h_twiddle[index];
                D_x_term[index] = std::complex<float>(0, -kVecNorm.x) * value_h_twiddle[index];
                D_z_term[index] = std::complex<float>(0, -kVecNorm.y) * value_h_twiddle[index];
            }

        // plans reused, not rebuilt — this is the fix
        fftwf_execute(p_height);
        fftwf_execute(p_slope_x);
        fftwf_execute(p_slope_z);
        fftwf_execute(p_D_x);
        fftwf_execute(p_D_z);

        int stride = N + 1;

        for (int n = 0; n < N; n++)
            for (int m = 0; m < M; m++) {
                int index = m * N + n;
                int rIndex = m * stride + n;
                float sign = ((m + n) % 2) ? -1.0f : 1.0f;

                normalField[rIndex] = glm::normalize(glm::vec3(
                    sign * out_slope_x[index][0],
                    -1.0f,
                    sign * out_slope_z[index][0]
                ));

                heightField[rIndex] = glm::vec3(
                    (n - N / 2) * L_x / N - sign * lambda * out_D_x[index][0],
                    sign * out_height[index][0],
                    (m - M / 2) * L_z / M - sign * lambda * out_D_z[index][0]
                );
            }

        for (int m = 0; m < M; m++) {
            int rSrc = m * stride + 0;
            int rDst = m * stride + N;
            heightField[rDst] = heightField[rSrc] + glm::vec3(L_x, 0.0f, 0.0f);
            normalField[rDst] = normalField[rSrc];
        }

        for (int n = 0; n <= N; n++) {
            int rSrc = 0 * stride + n;
            int rDst = M * stride + n;
            heightField[rDst] = heightField[rSrc] + glm::vec3(0.0f, 0.0f, L_z);
            normalField[rDst] = normalField[rSrc];
        }
    }

} // namespace ais_sim