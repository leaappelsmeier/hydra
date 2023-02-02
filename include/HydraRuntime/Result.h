#pragma once

namespace Hydra
{
  namespace Runtime
  {
    /// \brief Enum values for success and failure. To be used by functions as return values mostly, instead of bool.
    enum ResultEnum
    {
      HYDRA_FAILURE,
      HYDRA_SUCCESS
    };

    /// \brief Default enum for returning failure or success, instead of using a bool.
    struct [[nodiscard]] Result
    {
    public:
      Result(ResultEnum res)
        : e(res)
      {
      }

      void operator=(ResultEnum rhs) { e = rhs; }
      bool operator==(ResultEnum cmp) const { return e == cmp; }
      bool operator!=(ResultEnum cmp) const { return e != cmp; }

      bool Succeeded() const { return e == HYDRA_SUCCESS; }
      bool Failed() const { return e == HYDRA_FAILURE; }

      /// \brief Used to silence compiler warnings, when success or failure doesn't matter.
      void IgnoreResult()
      { /* dummy to be called when a return value is [[nodiscard]] but the result is not needed */
      }

    private:
      ResultEnum e;
    };
  } // namespace Runtime
} // namespace Hydra
