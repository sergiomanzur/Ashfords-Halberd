#include "audio_playback.hpp"
#include <limits>
#include <stdexcept>
namespace ashford {
namespace {
uint64_t add(uint64_t a, uint64_t b) {
  if (b > std::numeric_limits<uint64_t>::max() - a)
    throw std::runtime_error("SCEI playback clock overflow");
  return a + b;
}
uint64_t multiply(uint64_t a, uint64_t b) {
  if (b && a > std::numeric_limits<uint64_t>::max() / b)
    throw std::runtime_error("SCEI playback tick overflow");
  return a * b;
}
bool control(const SceEvent &e, uint8_t number) {
  return e.type == SceEventType::control && e.data1 == number;
}
} // namespace
bool ScheduledSceEvent::operator==(const ScheduledSceEvent &b) const {
  return frame == b.frame && index == b.index && event.tick == b.event.tick &&
         event.type == b.event.type && event.channel == b.event.channel &&
         event.data1 == b.event.data1 && event.data2 == b.event.data2 &&
         event.tempo == b.event.tempo;
}
SceSequencer::SceSequencer(SceSequence sequence)
    : sequence_(std::move(sequence)) {
  if (!sequence_.ppqn || sequence_.ppqn >= 0x8000 || sequence_.events.empty() ||
      sequence_.events.size() > sound_sequence_event_limit)
    throw std::runtime_error("Invalid native SCEI playback sequence");
  denominator_ = uint64_t(sequence_.ppqn) * 1000000;
  loops_.resize(sequence_.events.size());
  uint64_t previous = 0;
  for (size_t i = 0; i < sequence_.events.size(); ++i) {
    const auto &e = sequence_.events[i];
    if (e.tick < previous || e.channel >= 16 || e.data1 > 127 ||
        e.data2 > 127 ||
        (e.type == SceEventType::tempo && (!e.tempo || e.tempo > 0xffffff)) ||
        (e.type == SceEventType::end && i + 1 != sequence_.events.size()))
      throw std::runtime_error("Invalid SCEI playback event");
    previous = e.tick;
  }
  if (sequence_.events.back().type != SceEventType::end)
    throw std::runtime_error("SCEI playback lacks final end event");
  std::optional<Loop> start;
  uint8_t id = 0, channel = 0;
  for (size_t i = 0; i < sequence_.events.size(); ++i) {
    const auto &e = sequence_.events[i];
    if (control(e, 99)) {
      if (i + 1 >= sequence_.events.size() ||
          !control(sequence_.events[i + 1], 6) ||
          sequence_.events[i + 1].channel != e.channel)
        throw std::runtime_error("SCEI loop marker lacks loop number");
      const auto &number = sequence_.events[++i];
      if (e.data2 == 0) {
        if (start)
          throw std::runtime_error("Unsupported nested SCEI sequence loop");
        start = Loop{i + 1, number.tick};
        id = number.data2;
        channel = number.channel;
      } else if (e.data2 == 1) {
        if (!start || id != number.data2 || channel != number.channel ||
            i + 1 >= sequence_.events.size() ||
            !control(sequence_.events[i + 1], 38) ||
            sequence_.events[i + 1].channel != channel)
          throw std::runtime_error("Invalid SCEI loop end");
        const auto &count = sequence_.events[++i];
        if (count.data2)
          throw std::runtime_error("Unsupported finite SCEI sequence loop");
        if (count.tick <= start->tick)
          throw std::runtime_error("SCEI loop has zero duration");
        loops_[i] = start;
        start.reset();
      } else
        throw std::runtime_error("Unsupported SCEI loop marker");
    } else if (control(e, 6) || control(e, 38))
      throw std::runtime_error("SCEI loop controller without marker");
  }
  if (start)
    throw std::runtime_error("SCEI loop lacks end marker");
  schedule();
}
void SceSequencer::schedule() {
  const auto tick = sequence_.events.at(cursor_).tick;
  const auto numerator =
      multiply(multiply(tick - last_tick_, tempo_), sound_output_rate);
  due_ = add(due_, numerator / denominator_);
  fraction_ += numerator % denominator_;
  due_ = add(due_, fraction_ / denominator_);
  fraction_ %= denominator_;
  last_tick_ = tick;
}
std::vector<ScheduledSceEvent> SceSequencer::advance(uint32_t frames) {
  if (frames > sound_render_frame_limit)
    throw std::runtime_error("SCEI render frame budget exceeded");
  const uint64_t end = add(frame_, frames);
  std::vector<ScheduledSceEvent> events;
  while (frames && pending_) {
    const auto deadline = add(due_, fraction_ ? 1 : 0);
    if (deadline >= end)
      break;
    if (events.size() >= sound_scheduled_event_limit)
      throw std::runtime_error("SCEI scheduled event budget exceeded");
    const auto &e = sequence_.events[cursor_];
    events.push_back({deadline, cursor_, e});
    if (e.type == SceEventType::tempo)
      tempo_ = e.tempo;
    if (e.type == SceEventType::end) {
      pending_ = false;
      break;
    }
    if (loops_[cursor_]) {
      const auto loop = *loops_[cursor_];
      repetitions_ = add(repetitions_, 1);
      cursor_ = loop.target;
      last_tick_ = loop.tick;
    } else
      ++cursor_;
    schedule();
  }
  frame_ = end;
  return events;
}
} // namespace ashford
