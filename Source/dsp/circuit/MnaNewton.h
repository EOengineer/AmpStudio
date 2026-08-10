#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace circuit
{
/**
 * Tiny dense Newton–Raphson helper for realtime circuit islands.
 * Solves F(x)=0 with analytic Jacobian; falls back to damped steps.
 */
template <int N>
struct NewtonSolver
{
    int maxIterations = 12;
    float absTol = 1.0e-9f;
    float relTol = 1.0e-6f;

    struct Result
    {
        bool converged = false;
        int iterations = 0;
        float residualNorm = 0.0f;
    };

    /** F and J filled by caller for current x. Returns updated x in place. */
    template <typename ResidualFn>
    Result solve (std::array<float, N>& x, ResidualFn&& fillResidualAndJacobian) const
    {
        Result result;
        std::array<float, N> f {};
        std::array<float, N * N> j {};

        for (int iter = 0; iter < maxIterations; ++iter)
        {
            fillResidualAndJacobian (x, f, j);
            result.iterations = iter + 1;
            result.residualNorm = norm (f);

            if (result.residualNorm < absTol)
            {
                result.converged = true;
                return result;
            }

            std::array<float, N> dx {};
            if (! solveLinear (j, f, dx))
                return result;

            // damped: x -= alpha * dx  (f is residual; we solve J dx = f, step subtract)
            float alpha = 1.0f;
            std::array<float, N> xTrial = x;
            float bestNorm = result.residualNorm;

            for (int line = 0; line < 6; ++line)
            {
                for (int i = 0; i < N; ++i)
                    xTrial[i] = x[i] - alpha * dx[i];

                std::array<float, N> fTrial {};
                std::array<float, N * N> jUnused {};
                fillResidualAndJacobian (xTrial, fTrial, jUnused);
                const float n = norm (fTrial);
                if (n < bestNorm)
                {
                    bestNorm = n;
                    x = xTrial;
                    result.residualNorm = n;
                    if (n < absTol)
                    {
                        result.converged = true;
                        return result;
                    }
                    break;
                }
                alpha *= 0.5f;
            }

            if (alpha < 1.0e-3f)
                return result;
        }

        result.converged = result.residualNorm < absTol * 10.0f;
        return result;
    }

private:
    static float norm (const std::array<float, N>& v) noexcept
    {
        float s = 0.0f;
        for (float x : v)
            s += x * x;
        return std::sqrt (s);
    }

    static bool solveLinear (const std::array<float, N * N>& aIn,
                             const std::array<float, N>& bIn,
                             std::array<float, N>& xOut)
    {
        // Gaussian elimination with partial pivot (N is tiny: 1–3)
        std::array<float, N * N> a = aIn;
        std::array<float, N> b = bIn;

        for (int col = 0; col < N; ++col)
        {
            int pivot = col;
            float best = std::abs (a[(size_t) col * N + (size_t) col]);
            for (int r = col + 1; r < N; ++r)
            {
                const float v = std::abs (a[(size_t) r * N + (size_t) col]);
                if (v > best)
                {
                    best = v;
                    pivot = r;
                }
            }

            if (best < 1.0e-20f)
                return false;

            if (pivot != col)
            {
                for (int c = 0; c < N; ++c)
                    std::swap (a[(size_t) pivot * N + (size_t) c], a[(size_t) col * N + (size_t) c]);
                std::swap (b[(size_t) pivot], b[(size_t) col]);
            }

            const float diag = a[(size_t) col * N + (size_t) col];
            for (int r = col + 1; r < N; ++r)
            {
                const float f = a[(size_t) r * N + (size_t) col] / diag;
                for (int c = col; c < N; ++c)
                    a[(size_t) r * N + (size_t) c] -= f * a[(size_t) col * N + (size_t) c];
                b[(size_t) r] -= f * b[(size_t) col];
            }
        }

        for (int i = N - 1; i >= 0; --i)
        {
            float s = b[(size_t) i];
            for (int c = i + 1; c < N; ++c)
                s -= a[(size_t) i * N + (size_t) c] * xOut[(size_t) c];
            xOut[(size_t) i] = s / a[(size_t) i * N + (size_t) i];
        }

        return true;
    }
};
} // namespace circuit
