// Reference · Invite — "14 days full, then invite 1 person" for FREE Reference plugins.
// Pure C++17, no JUCE: the plugin passes in the device id, the time and the saved files.
//
//   Ali's plugin shows his invite code     SLK-7K2Q-XM
//   Reza types Ali's code in his plugin -> a thank-you code made only for Ali
//   Ali types the thank-you code         -> full version forever on that computer
//
// This is NOT a license generator. The built-in key only signs invite / thank-you codes.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace refinv
{
inline constexpr int kTrialDays = 14;
inline constexpr int kInvitesNeeded = 1;
inline constexpr int64_t kDay = 24 * 60 * 60;

// ---- crypto helpers (exposed for tests) ----
std::string sha256Hex (const std::string& data);
std::vector<uint8_t> hmacSha256 (const std::vector<uint8_t>& key, const std::string& msg);

/** Upper-case, keep only alphabet characters (drops spaces, dashes...). */
std::string normalise (const std::string& code);
/** Groups for display: "SLK7K2QXM" -> "SLK-7K2Q-XM", "ABCD123456" -> "ABCDE-12345". */
std::string pretty (const std::string& code);

struct State
{
    int64_t start = 0;      // first run (unix seconds)
    int64_t last = 0;       // latest time ever seen
    std::string gaveFor;    // the invite code this install already thanked (one per install)
    std::string unlock;     // unlock token, only valid on this computer
    std::vector<std::string> thanks; // thank-you codes accepted so far
};

enum class GiveResult { ok, empty, badFormat, ownCode, alreadyGaveOther };
enum class RedeemResult { ok, empty, badFormat, ownCode, wrongCode, alreadyUsed };

class Invite
{
public:
    /** deviceId: any stable per-computer string. prefix: 3 letters, e.g. "SLK". */
    Invite (const std::string& deviceId, const std::string& productId, const std::string& prefix);

    const std::string& computerId() const { return cid; }
    const std::string& inviteCode() const { return invite; } // normalised, e.g. SLK7K2QXM
    const std::string& tag() const { return myTag; }

    /** Load from the saved copies (any may be empty or broken). Never lets time go backwards. */
    void load (const std::vector<std::string>& copies, int64_t now);
    /** Text to write to every copy. */
    std::string save() const;

    bool unlocked() const;
    int daysLeft() const;      // 0 when the trial is over
    bool demo() const { return ! unlocked() && daysLeft() <= 0; }
    int invitesDone() const { return (int) state.thanks.size(); }

    /** Reza's side: make the thank-you code for a friend's invite code. */
    GiveResult giveThanks (const std::string& friendInvite, std::string& thanksOut);
    /** Ali's side: type a thank-you code. */
    RedeemResult redeem (const std::string& thanksCode);

    const State& raw() const { return state; }
    bool wasTampered() const { return tampered; }

    // pure functions, also used by tests
    std::string inviteFor (const std::string& computerId) const;
    std::string tagFor (const std::string& computerId) const;
    std::string thanksFor (const std::string& inviteCode, const std::string& helperTag) const;
    std::string unlockFor (const std::string& computerId) const;

private:
    std::string mac (const std::string& msg, size_t chars) const;
    bool parse (const std::string& text, State& out) const;

    std::string prefix, cid, invite, myTag;
    State state;
    int64_t now = 0;
    bool tampered = false;
};
} // namespace refinv
