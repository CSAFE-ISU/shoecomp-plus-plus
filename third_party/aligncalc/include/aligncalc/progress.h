#ifndef ALIGNCALC_PROGRESS_H
#define ALIGNCALC_PROGRESS_H

namespace AlignCalc
{
    // Progress and cancellation for the long-running calls (graph
    // building, clique search, distinct alignments). The defaults
    // report nothing and never cancel.
    class Progress
    {
       public:
        virtual ~Progress() = default;

        // |fraction| of the whole call in [0, 1], non-decreasing, and
        // a short stage text. Called only from the thread that called
        // into the library, in steps of at most 5% per stage.
        virtual void report(float fraction, const char* stage)
        {
            (void)fraction;
            (void)stage;
        }

        // Polled often from every worker thread: must be thread-safe
        // and cheap. Once true, the call stops early and returns
        // Status::Cancelled.
        virtual bool cancelled() const { return false; }
    };

    // How a call ended.
    enum class Status
    {
        Ok,
        NoMatch,    // ran to the end, nothing reached the bounds
        Cancelled,  // Progress::cancelled()
        Error,      // invalid input or parameters, failed fit
    };

    namespace detail
    {
        // Forwards cancelled() only, for repeated stages whose
        // progress would run backwards (later distinct rounds).
        class CancelOnly : public Progress
        {
            const Progress& inner_;

           public:
            explicit CancelOnly(const Progress& inner) : inner_(inner)
            {
            }
            bool cancelled() const override
            {
                return inner_.cancelled();
            }
        };
    }  // namespace detail
}  // namespace AlignCalc

#endif
