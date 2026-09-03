/* Runtime-only possession and domination; never serialized. */
#ifndef CONTROL_H
#define CONTROL_H

enum control_mode { CONTROL_IMMORTAL_PUPPET, CONTROL_SPELL_PUPPET, CONTROL_MIND_CONTROL };
struct control_session {
  enum control_mode mode;
  struct char_data *controller, *target;
  struct descriptor_data *driver, *observer;
  struct event *expiry;
  int executing, ended;
  unsigned long ready_at;
  struct control_session *next;
};
void switch_to_char(struct char_data *ch, struct char_data *victim);
int start_control_session(struct char_data *ch, struct char_data *victim, enum control_mode mode, int seconds);
void end_control_session(struct control_session *session);
void end_character_control(struct char_data *ch);
void end_descriptor_control(struct descriptor_data *d);
void end_all_control(void);
int control_input(struct descriptor_data *d, char *input);
int control_command_allowed(struct char_data *ch, const char *verb);
int control_order(struct char_data *ch, char *argument);
int control_body_abandoned(struct char_data *ch);
int control_attack_allowed(struct char_data *ch, struct char_data *victim);
struct descriptor_data *control_output_peer(struct descriptor_data *d);
void cast_control(struct char_data *ch, struct char_data *victim, int spellnum);
ACMD(do_puppet);
ACMD(do_unpuppet);
ACMD(do_socket);
#endif
