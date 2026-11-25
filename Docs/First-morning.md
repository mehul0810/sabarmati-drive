# First morning in Ahmedabad

Open **Play Sabarmati Drive.command** or run `bash Scripts/run-mac.sh --standalone` if the local Mac package is available. The game opens in a fitted window. A new riverfront player wakes at 07:15 in Flat 2B at Kesar Apartments with ₹1,00,000 in cash and ₹5,00,000 in the bank.

To play the updated development build, run `bash Scripts/run-mac.sh --game --no-build` from the checkout. Press **E** beside the bed to sleep until 07:15. The resident rolls over, sits on the mattress edge, plants both feet and stands. **Esc** ends the wake-up and opens the pause menu.

Walk up to the wardrobe and press **E** to see clothing and accessories, change the avatar between male and female, or edit complexion and hair. Each item shows whether you are wearing it, own it, or can buy it at Sutra. At the apartment mirror, select a piece to preview it immediately. For an owned top, **Wear now** plays the change and saves it when the gesture finishes. **Esc** during the change restores the outfit you arrived in. Switching between the sand and indigo shirts keeps your jeans and equipped watch.

Follow the apartment stairs to the street. Sutra Cloth House is on the ground floor. Press **E** at the counter to try on the indigo cotton shirt, terracotta kurta or copper wristwatch. The till shows the item, price, payment account and receipt. Use **I** during play to view cash, bank balance and your latest receipt. Use **Esc** or the close button to leave a panel. **Esc → Quit Game** saves your location and closes the app.

Visit nearby Kagaz Print House to pick up a print order. Deliver it on foot to the Nadi Chai counter to earn ₹850 in the bank. The shop pays once. On the promenade, press **E** when “Say hello” appears to hear from the family or visiting artists through an on-screen caption.

Existing profiles keep their display name, discoveries and best River Run time when upgrading. Starting funds are granted once. Outfit, complexion, accessory, receipts and the print order live in the same offline Unreal save. On foot, the game periodically records a grounded checkpoint. When driving, a checkpoint uses a walkable spot beside the vehicle. The parked car and moving crowd restart with the world.

## Mac acceptance route

The first-morning review uses the normal title and shop controls, character collision, a separate slot and a separate user directory. Run both commands with the same new 32-character hexadecimal GUID and absolute test directory. Wait for the first window to finish and quit before starting the second.

```bash
review_id="$(uuidgen | tr -d '-')"
review_dir="$(mktemp -d /tmp/sabarmati-playtest.XXXXXXXX)"
bash Scripts/run-mac.sh --standalone -- "-UserDir=$review_dir" -SabarmatiMorningReview "-SabarmatiMorningQAId=$review_id"
# Once the first window has closed
bash Scripts/run-mac.sh --standalone -- "-UserDir=$review_dir" -SabarmatiMorningReview "-SabarmatiMorningQAId=$review_id" -SabarmatiMorningContinue -SabarmatiMorningDelivery
```

Add `-SabarmatiMorningFittingReview` on the delivery run to exercise a bank purchase of the wristwatch and a cash purchase of the kurta at the counter. The game writes PNGs and measurements under `Saved/MorningReviews` in the chosen test directory. The ordinary launcher opens the player's normal local profile. Both Finder and the two window-size shortcuts use the same Application Support profile. The fitting review moves the test character back to the clothing counter after the delivery so that the accessory and second outfit can be checked separately.

## Packaged gameplay on the M1 Max

Gameplay revision `b1bbcd3` was built with UE 5.8.4 for native arm64 and Metal SM5. In a 1600×900 window, the bedside camera travelled 77.16 cm before giving control to the player. The character walked 24.48 m from the bedroom through the wardrobe, stairs and street to the counter, with 676 moving frames and a 16.55 ms p95. Both hair choices changed the character on screen and survived a save readback. [Captures from the packaged game](captures/first-morning.jpg)

At 960×540 the same route covered 24.51 m. The character and wardrobe controls were both visible, the shirt purchase completed and all three relatives crossed. The Mac keeps the original 1600×900 captures and the smaller run in `Saved/MorningReviews/78AC5548652F4A0BB55C3CFF78AC6901` and `Saved/MorningReviews/EFFA299E9EDE441895481E7BB06BC640` respectively.

The indigo shirt cost ₹1,250. A separate process continued at the counter with the shirt equipped, ₹98,750 in cash and ₹5,00,000 in the bank. The character then carried the print order 31.96 m and earned ₹850. A repeated interaction kept the delivery count at one. The separate fitting check bought the ₹1,800 watch from the bank and the ₹2,200 kurta with cash, leaving ₹96,550 in cash and ₹4,99,050 in the bank. The review inspected the new skeletal mesh and the watch attached to the animated wrist.

All three relatives waited at the kerb, crossed and visited the shop. The observed family displacement was 34.31 m. Both individual visitors moved, and the visiting printmaker reached the counter. The native Unreal tests `Sabarmati.FirstMorning.OfflineProfile` and `Sabarmati.FirstMorning.FinderProfile` passed, along with 29 project checks.

## Rendering and sustained performance

The two short night drives used the same compiled riverfront game and M1 Max. The only difference in recorded settings was the drawable size. Both used TAA, 100% primary and secondary scale, fixed resolution, frame smoothing on, and VSync and frame cap off. The game checked the road beneath the car on each chase frame.

| Windowed night drive | Chase frames | Sample and distance | Median | p95 | p99 |
|---|---:|---|---:|---:|---:|
| Standard, 1600×900 | 334 | 5.376 s, 45.01 m | 16.12 ms | 16.67 ms | 16.77 ms |
| Fitted Retina, 3280×1845 | 155 | 5.428 s, 45.44 m | 35.27 ms | 45.31 ms | 47.11 ms |

The standard and Retina runs recorded road support on 334/334 and 155/155 chase frames. Their raw measurements and PNGs are under `Saved/RenderingBenchmarks/20261123-171737-EC65A2CE` and `Saved/RenderingBenchmarks/20261123-171841-858F2E39`. The Retina run used the Mac’s available desktop area. Sustained 3840×2160 remains unmeasured.

The cooked app then completed **1,200.016 seconds at 1568×882** on the M1 Max. macOS reported AC power before and after the run. Across 28 loops the player walked 1,766.47 m and drove 5,426.52 m. The 72,171 wall-frame samples had a median of 16.15 ms, p95 of 16.74 ms and p99 of 34.95 ms. There were 743 frames over 33.33 ms and eight over 100 ms. The worst took 315.48 ms. First- and last-minute p95 were 16.90 and 16.36 ms.

A normal 1280×720 game window was open on the same Mac. Unreal measured 4,446 focused frames at p95 16.89 ms and 67,725 background frames at p95 16.60 ms. This run establishes mixed-focus endurance. The earlier attempt was interrupted by a pause and resume during a driving loop and failed its reset check. Only the completed repeat was counted. See `Saved/StandaloneBenchmarks/C28B570E-D3A5-44F7-A3FA-8E01BA22EA20` for the raw data, frame-time report, power readings and window observations. Only the M1 Max was available for testing.

The local arm64 Development ZIP is `Saved/MacPackage/Sabarmati-Drive-Mac-arm64.zip`, 444,639,833 bytes with SHA-256 `3836fd475571276b4e5f13340cd6c6249a4b0fc5b0095fc6027159d8ac4a8c9c`.

## Art and world scope

Kesar Apartments and Sutra Cloth House are original buildings. The riverfront team used satellite imagery to study the west bank’s block spacing, roofs and service lanes, then used credited stone and asphalt scans for the close surfaces. Atal Bridge, the crossing and the rear lanes are useful places to inspect the result from street level. [Map and satellite references](../SourceAssets/AHMEDABAD-CITY-SOURCES.md) · [Surface and model sources](../SourceAssets/RIVERFRONT-SOURCES.md)

The family has three adult relatives. The two visiting artists take separate routes and have their own activities and greetings. The public characters share two base bodies. Close-up facial expression, tailoring, vehicle bodywork and individual buildings across the wider city still need an art pass to reach the warm photoreal concept.
