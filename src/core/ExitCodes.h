#pragma once

namespace papercut {

// CLI codes match Android.
enum ExitCode {
    Ok = 0,
    Usage = 1,
    OpenFailed = 2,     // open_failed
    Password = 3,       // PASSWORD / wrong password
    NotSupported = 4,   // NOT_PDF / unreadable
    WriteFailed = 5,    // EXPORT / FOLIO_COPY
};

} // namespace papercut
