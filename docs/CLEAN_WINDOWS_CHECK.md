# Clean Windows release check

This check must be recorded on an actual clean Windows account/VM. It has not
been established by sanitizing processes on the developer's machine.

1. Copy the candidate ZIP and its recorded SHA-256 to Windows x64 without the
   development SDK, Python, Torch, repository paths or NVIDIA key.
2. Verify the ZIP hash and extract it to a folder with spaces and Unicode in its
   path, for example `C:\Demo apps\Breakwater α`. Keep the source ZIP/notices with
   the candidate. Do not copy an old `.env`, database or user configuration.
3. Disconnect networking for the offline check. Double-click
   `Verify-Breakwater.cmd`. It uses bundled Python; neither PowerShell 7 nor an
   installed Python/Qt SDK is required. Retain the newly created sibling
   `verification-...` directory, its JSON, logs and screenshots.
4. Double-click `Start-Breakwater.cmd`. Run the Sandbox forecast and local search,
   compare trajectories, inspect review, restart and reopen actual saved history.
5. Load both bundled Operations examples and inspect their original units,
   cutoff, plan comparison and independent validation. Open the recorded Nemotron
   example while disconnected. It must remain historical and cannot authorize
   actions. A fresh hosted request must report missing credentials accurately.
6. Check 100%, 125%, 150% and 200% display scaling. Resize to the supported
   960×540 client minimum and verify close buttons, menus, scrollbars and keyboard
   focus. Do not infer a clean-machine result from screenshot-only tests.

Record Windows build, CPU/GPU/driver, display resolution/scaling, candidate/source
hashes, exact steps, failures, counts and restart behavior. Optional Science is a
separate installation/qualification. A missing Torch capability in the basic
bundle is expected, not an application failure.

Publish no binary until the ownership/license/notices/source-delivery and clean
Windows gates in LICENSING and PORTFOLIO_RELEASE are satisfied.
