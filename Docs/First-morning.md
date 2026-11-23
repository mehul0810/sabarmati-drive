# First morning in Ahmedabad

Open **Play Sabarmati Drive.command** or run `bash Scripts/run-mac.sh --standalone` if the local Mac package is available. The game opens in a fitted window. A new riverfront player wakes at 07:15 in Flat 2B at Kesar Apartments with ₹1,00,000 in cash and ₹5,00,000 in the bank.

Walk up to the wardrobe and press **E** to change complexion or haircut, or wear a purchased item. Follow the apartment stairs to the street. Sutra Cloth House is on the ground floor. Press **E** at the counter to try on the indigo cotton shirt, terracotta kurta or copper wristwatch. The till shows the item, price, payment account and receipt. Use **I** during play to view cash, bank balance and your latest receipt. Use **Esc** or the close button to leave a panel. **Esc → Quit Game** saves your location and closes the app.

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

## Art and world scope

Kesar Apartments and Sutra Cloth House are original buildings. The riverfront team used satellite imagery to study the west bank’s block spacing, roofs and service lanes, then used credited stone and asphalt scans for the close surfaces. Atal Bridge, the crossing and the rear lanes are useful places to inspect the result from street level. [Map and satellite references](../SourceAssets/AHMEDABAD-CITY-SOURCES.md) · [Surface and model sources](../SourceAssets/RIVERFRONT-SOURCES.md)

The family has three adult relatives. The two visiting artists take separate routes and have their own activities and greetings. The public characters share two base bodies. Close-up facial expression, tailoring, vehicle bodywork and individual buildings across the wider city still need an art pass to reach the warm photoreal concept.
