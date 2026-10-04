# ChordForge
Expressive 8-bar chord progression generator (VST3). Pick key + major/minor, hit NEW IDEA, then drag the MIDI pad into FL Studio.

## Get the Windows .vst3 (no compiler needed)
1. Create a new GitHub repo and upload everything in this folder (keep the .github folder).
2. Open the Actions tab, run "Build ChordForge" (it also runs on every push). Takes ~10 min.
3. Download the `ChordForge-VST3-Windows` artifact and unzip it.
4. Copy `ChordForge.vst3` to `C:\Program Files\Common Files\VST3\`.
5. In FL Studio: Options > Manage plugins > Find installed plugins.

## Use
Load ChordForge on a channel, choose Key/Mode/Richness, press NEW IDEA until you like it (PLAY previews it),
then drag the pink pad onto your synth's piano roll, the playlist, or a channel.
