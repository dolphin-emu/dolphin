# Slippi Jukebox

Slippi Jukebox is a built-in solution to play Melee's OST in a manner that's effectively detached from emulation.

## Features

- Menu music
- All stage music
- Alternate stage music<sup>1</sup>
- "Versus" splash screen jingle
- Ranked stage striking music 
- Auto-ducking music volume when pausing the game
- _Break the Targets_ + _Home Run Contest_ music
- Controlling volume with the in-game options and Dolphin's audio volume slider
- Lottery menu music
- Single player mode<sup>2</sup>

1. _Alternate stage music has a 12.5% chance of playing. Holding triggers to force alternate tracks to play is not supported._

2. _In some 1P modes, stage music will differ from vs mode. This behavior is not supported by jukebox. Additionally, the following songs will not play:_
	<ul>
		<li>Classic "Stage Clear" Jingle</li>
		<li>Classic "Continue?" and "Game Over" Jingles</li>
		<li>Credits music</li>
		<li>Post-credits congratulations fmv music</li>
	</ul>

## How it works

When a `Jukebox` instance is created (generally from an EXI Device), it scans the iso for music files and stores their locations + file sizes in a hashmap.

Two child threads are immediately spawned: `JukeboxMessageDispatcher` and `JukeboxMusicPlayer`. Together these threads form an event loop.

The message dispatcher continuously reads from dolphin's game memory and dispatches relevant events. The music player thread listens to the events and handles them by decoding the appropriate music files and playing them with the default audio device.

When the `Jukebox` instance is dropped, the child threads are instructed to terminate, the event loop breaks, and the music stops.

## Decoding Melee's Music

The logic for decoding Melee's music has been split out into a public library. See the [`hps_decode`](https://crates.io/crates/hps_decode) crate for more. For general information about the `.hps` file format, [see here.](https://github.com/DarylPinto/hps_decode/blob/main/HPS-LAYOUT.md)
