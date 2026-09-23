// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PRISM_INPUT_H
#define GAME_CLIENT_PRISM_INPUT_H

namespace PrismInput
{
// One instance per physical connection, shared by active and dummy input paths.
// Physical counters never include synthetic edges; wire counters never rewind.
class CFireComposer
{
 int m_LastManual = 0;
 int m_Output = 0;
 bool m_Owned = false;

public:
 void Reset(int Manual, int Mask)
 {
  m_LastManual = m_Output = Manual & Mask;
  m_Owned = false;
 }
 void RebaseSource(int Manual, int Mask) { m_LastManual = Manual & Mask; }
 bool Owned() const { return m_Owned; }
 int Compose(int Manual, bool Pulse, int Mask)
 {
  Manual &= Mask;
  const int Delta = (Manual - m_LastManual) & Mask;
  m_LastManual = Manual;
  m_Output = (m_Output + Delta) & Mask;
  // A physical press taking over a synthetic hold is not another press.
  if(m_Owned && Delta > 0)
   m_Output = (m_Output - 1) & Mask;
  // Consecutive pulses require a release edge between presses.
  if(Pulse && m_Owned && Delta == 0 && !(Manual & 1))
   m_Output = (m_Output + 1) & Mask;
  const bool Held = (Manual & 1) || Pulse;
  if(bool(m_Output & 1) != Held)
   m_Output = (m_Output + 1) & Mask;
  m_Owned = Pulse && !(Manual & 1);
  return m_Output;
 }
};
}
#endif
