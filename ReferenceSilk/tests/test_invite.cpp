// Reference · Invite — tests (no JUCE)
#include "../Source/Invite/RefInvite.h"
#include <cstdio>

using namespace refinv;
static int failures = 0;
#define CHECK(cond, ...) do { const bool ok_ = (cond); std::printf ("  %s  ", ok_ ? "ok  " : "FAIL"); std::printf (__VA_ARGS__); std::printf ("\n"); if (! ok_) ++failures; } while (0)

static const int64_t T0 = 1790000000; // some day in 2026

int main()
{
    std::printf ("Reference invite tests\n");

    // crypto known answers
    CHECK (sha256Hex ("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 of \"abc\"");
    CHECK (sha256Hex ("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 of empty text");
    {
        std::vector<uint8_t> k (20, 0x0b);
        auto h = hmacSha256 (k, "Hi There");
        char hex[65]; for (int i = 0; i < 32; ++i) std::snprintf (hex + i * 2, 3, "%02x", h[(size_t) i]);
        CHECK (std::string (hex) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "HMAC-SHA256 RFC 4231 case 1");
    }

    Invite ali ("ALI-PC-DEVICE", "silk", "SLK"), reza ("REZA-PC-DEVICE", "silk", "SLK"), sara ("SARA-PC", "silk", "SLK");
    ali.load ({}, T0); reza.load ({}, T0); sara.load ({}, T0);

    CHECK (ali.inviteCode().size() == 9 && ali.inviteCode().rfind ("SLK", 0) == 0, "invite code looks like %s", pretty (ali.inviteCode()).c_str());
    CHECK (ali.inviteCode() != reza.inviteCode(), "different computers get different invite codes");
    {
        Invite again ("ALI-PC-DEVICE", "silk", "SLK");
        CHECK (again.inviteCode() == ali.inviteCode() && again.computerId() == ali.computerId(), "same computer -> same id and code");
        Invite other ("ALI-PC-DEVICE", "eq", "SLK");
        CHECK (other.computerId() != ali.computerId(), "computer id depends on the product");
    }

    // 14-day timer
    CHECK (ali.daysLeft() == 14 && ! ali.demo() && ! ali.unlocked(), "first run: 14 days left, full");
    {
        Invite a ("ALI-PC-DEVICE", "silk", "SLK");
        a.load ({ ali.save() }, T0 + 2 * kDay + 5);
        CHECK (a.daysLeft() == 12, "after 2 days: %d days left", a.daysLeft());
        a.load ({ ali.save() }, T0 + 14 * kDay + 1);
        CHECK (a.daysLeft() == 0 && a.demo(), "after 14 days: demo");
    }

    // Windows line endings in the saved file
    {
        std::string crlf;
        for (char ch : ali.save()) { if (ch == '\n') crlf += '\r'; crlf += ch; }
        Invite a ("ALI-PC-DEVICE", "silk", "SLK");
        a.load ({ crlf }, T0 + kDay);
        CHECK (! a.wasTampered() && a.daysLeft() == 13, "file saved with Windows line endings still reads fine");
    }

    // clock moved back
    {
        Invite a ("ALI-PC-DEVICE", "silk", "SLK");
        a.load ({ ali.save() }, T0 + 10 * kDay);
        const auto saved = a.save();
        Invite b ("ALI-PC-DEVICE", "silk", "SLK");
        b.load ({ saved }, T0 + 1 * kDay); // user moved the clock 9 days back
        CHECK (b.daysLeft() == 4, "clock moved back: still %d days left (no extra days)", b.daysLeft());
    }

    // two copies: the earliest start wins
    {
        Invite later ("ALI-PC-DEVICE", "silk", "SLK");
        later.load ({}, T0 + 13 * kDay);
        Invite a ("ALI-PC-DEVICE", "silk", "SLK");
        a.load ({ later.save(), ali.save() }, T0 + 13 * kDay);
        CHECK (a.daysLeft() == 1, "deleting one copy does not reset: %d day left", a.daysLeft());
    }

    // edited file
    {
        auto s = ali.save();
        auto p = s.find ("start=");
        s[p + 6] = s[p + 6] == '9' ? '8' : '9';
        Invite a ("ALI-PC-DEVICE", "silk", "SLK");
        a.load ({ s }, T0 + 1 * kDay);
        CHECK (a.wasTampered() && a.demo(), "an edited file gives no trial");
    }

    // Reza helps Ali
    std::string thanks;
    CHECK (reza.giveThanks (pretty (ali.inviteCode()), thanks) == GiveResult::ok, "Reza makes a thank-you code: %s", thanks.c_str());
    CHECK (thanks.size() == 11, "thank-you code shape 5-5");

    // case / space / dash insensitive
    std::string lower;
    for (char c : thanks) lower += (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
    std::string spaced;
    for (char c : lower) if (c != '-') { spaced += c; spaced += ' '; }

    Invite a ("ALI-PC-DEVICE", "silk", "SLK");
    a.load ({ ali.save() }, T0 + 20 * kDay);
    CHECK (a.demo(), "Ali is in demo before the code");
    CHECK (a.redeem ("ABCDE-FGHJK") == RedeemResult::wrongCode, "a made-up code is refused");
    CHECK (a.redeem ("12345") == RedeemResult::badFormat, "a short code is refused");
    {
        std::string own;
        Invite selfHelper ("ALI-PC-DEVICE", "silk", "SLK");
        selfHelper.load ({}, T0);
        CHECK (selfHelper.giveThanks (ali.inviteCode(), own) == GiveResult::ownCode, "cannot thank your own invite code");
    }
    {
        // Ali's other plugin install on the same computer cannot thank Ali (same tag)
        Invite twin ("ALI-PC-DEVICE", "silk", "SLK");
        twin.load ({}, T0);
        const auto fake = twin.tag() + twin.thanksFor (ali.inviteCode(), twin.tag());
        CHECK (a.redeem (fake) == RedeemResult::ownCode, "a code made on your own computer is refused");
    }
    {
        // a thank-you code made for Sara does not work for Ali
        std::string forSara;
        Invite helper ("HELPER2", "silk", "SLK");
        helper.load ({}, T0);
        helper.giveThanks (sara.inviteCode(), forSara);
        CHECK (a.redeem (forSara) == RedeemResult::wrongCode, "a code made for someone else is refused");
    }
    CHECK (a.redeem (spaced) == RedeemResult::ok, "lower case with spaces works: \"%s\"", spaced.c_str());
    CHECK (a.unlocked() && ! a.demo(), "Ali is unlocked");

    // unlocked survives save / load
    const auto unlockedFile = a.save();
    {
        Invite b ("ALI-PC-DEVICE", "silk", "SLK");
        b.load ({ unlockedFile }, T0 + 400 * kDay);
        CHECK (b.unlocked() && ! b.demo(), "still unlocked after a year");
    }
    // copied to another computer
    {
        Invite thief ("OTHER-PC", "silk", "SLK");
        thief.load ({ unlockedFile }, T0 + 30 * kDay);
        CHECK (! thief.unlocked() && thief.demo(), "unlock file copied to another computer does not unlock");
    }

    // one thank-you per install
    {
        std::string second;
        CHECK (reza.giveThanks (sara.inviteCode(), second) == GiveResult::alreadyGaveOther, "Reza cannot thank a second person");
        CHECK (second == thanks, "...and is shown the same code again");
        std::string again;
        CHECK (reza.giveThanks (ali.inviteCode(), again) == GiveResult::ok && again == thanks, "asking again for Ali gives the same code");
        Invite r2 ("REZA-PC-DEVICE", "silk", "SLK");
        r2.load ({ reza.save() }, T0 + kDay);
        std::string third;
        CHECK (r2.giveThanks (sara.inviteCode(), third) == GiveResult::alreadyGaveOther, "...also after restarting the plugin");
    }
    {
        std::string bad;
        CHECK (sara.giveThanks ("XYZ-1234", bad) == GiveResult::badFormat, "a wrong invite code shape is refused");
        CHECK (sara.giveThanks ("  ", bad) == GiveResult::empty, "an empty invite field is refused");
    }

    std::printf (failures == 0 ? "\nALL INVITE TESTS PASSED\n" : "\n%d INVITE TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
