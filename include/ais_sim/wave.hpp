#pragma once
#include <glm/glm.hpp>
#include <complex>
#include <random>
#include <cmath>
#include <fftw3.h>

namespace ais_sim {

    class Wave {
    public:
        Wave(int N, int M, float L_x, float L_z, glm::vec2 omega, float V, float A, float lambda);
        ~Wave();

        Wave(const Wave&) = delete;
        Wave& operator=(const Wave&) = delete;

        Wave(Wave&& other) noexcept;
        Wave& operator=(Wave&& other) noexcept;

        void buildField(float time);

        glm::vec3* heightField = nullptr;
        glm::vec3* normalField = nullptr;
        int N, M;

    private:
        std::complex<float>* value_h_twiddle_0 = nullptr;
        std::complex<float>* value_h_twiddle_0_conj = nullptr;
        std::complex<float>* value_h_twiddle = nullptr;

        // persistent FFT input buffers (plans are bound to these addresses)
        std::complex<float>* slope_x_term = nullptr;
        std::complex<float>* slope_z_term = nullptr;
        std::complex<float>* D_x_term = nullptr;
        std::complex<float>* D_z_term = nullptr;

        // persistent FFT output buffers
        fftwf_complex* out_height = nullptr;
        fftwf_complex* out_slope_x = nullptr;
        fftwf_complex* out_slope_z = nullptr;
        fftwf_complex* out_D_x = nullptr;
        fftwf_complex* out_D_z = nullptr;

        // plans, built once in the constructor
        fftwf_plan p_height = nullptr;
        fftwf_plan p_slope_x = nullptr;
        fftwf_plan p_slope_z = nullptr;
        fftwf_plan p_D_x = nullptr;
        fftwf_plan p_D_z = nullptr;

        std::default_random_engine generator;
        std::normal_distribution<float> normal_dist;

        const float g = 9.8f;
        const float l = 0.1f;
        float A, V, lambda;
        int kNum;
        glm::vec2 omega_hat;
        float L_x, L_z;

        //cache
        glm::vec2* kVecCache = nullptr;
        float* omegaCache = nullptr;

        float func_omega(float k) const;
        float func_P_h(glm::vec2 vec_k) const;
        std::complex<float> func_h_twiddle_0(glm::vec2 vec_k);
        std::complex<float> func_h_twiddle(int kn, int km, float t) const;
    };

} // namespace ais_sim