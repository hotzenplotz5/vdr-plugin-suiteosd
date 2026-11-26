#ifndef SUITEOSD_PAIRING_DIALOG_H
#define SUITEOSD_PAIRING_DIALOG_H

namespace suiteosd {
// Presentation contract between the native cOsdMenu owner and passive Suite skin.
// Other VDR skins still receive ordinary, navigable menu rows.
inline constexpr char kPairingDialogTitle[]="VDR Suite – Gerätekopplung";
inline constexpr char kPairingCodePrefix[]="Kopplungscode: ";
enum PairingDialogRow { PairCode,PairStatus,PairHelp1,PairHelp2,PairCheck,PairTest,PairBack,PairRows };
}
#endif
