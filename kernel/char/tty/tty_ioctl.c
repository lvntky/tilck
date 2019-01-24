/* SPDX-License-Identifier: BSD-2-Clause */

#include <tilck/common/basic_defs.h>
#include <tilck/common/string_util.h>
#include <tilck/common/debug/termios_debug.c.h>

#include <tilck/kernel/fs/vfs.h>
#include <tilck/kernel/fs/devfs.h>
#include <tilck/kernel/errno.h>
#include <tilck/kernel/user.h>
#include <tilck/kernel/term.h>

#include <termios.h>      // system header
#include <fcntl.h>        // system header
#include <sys/ioctl.h>    // system header
#include <linux/kd.h>     // system header

#include "tty_int.h"

const struct termios default_termios =
{
   .c_iflag = ICRNL | IXON,
   .c_oflag = OPOST | ONLCR,
   .c_cflag = CREAD | B38400 | CS8,
   .c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN,
   .c_line = 0,

   .c_cc =
   {
      [VINTR]     = 0x03,        /* typical value for TERM=linux, Ctrl+C */
      [VQUIT]     = 0x1c,        /* typical value for TERM=linux, Ctrl+\ */
      [VERASE]    = 0x7f,        /* typical value for TERM=linux */
      [VKILL]     = 0x15,        /* typical value for TERM=linux, Ctrl+7 */
      [VEOF]      = 0x04,        /* typical value for TERM=linux, Ctrl+D */
      [VTIME]     = 0,           /* typical value for TERM=linux (unset) */
      [VMIN]      = 0x01,        /* typical value for TERM=linux */
      [VSWTC]     = 0,           /* typical value for TERM=linux (unsupported)*/
      [VSTART]    = 0x11,        /* typical value for TERM=linux, Ctrl+Q */
      [VSTOP]     = 0x13,        /* typical value for TERM=linux, Ctrl+S */
      [VSUSP]     = 0x1a,        /* typical value for TERM=linux, Ctrl+Z */
      [VEOL]      = 0,           /* typical value for TERM=linux (unset) */
      [VREPRINT]  = 0x12,        /* typical value for TERM=linux, Ctrl+R */
      [VDISCARD]  = 0x0f,        /* typical value for TERM=linux, Ctrl+O */
      [VWERASE]   = 0x17,        /* typical value for TERM=linux, Ctrl+W */
      [VLNEXT]    = 0x16,        /* typical value for TERM=linux, Ctrl+V */
      [VEOL2]     = 0            /* typical value for TERM=linux (unset) */
   }
};

void tty_update_special_ctrl_handlers(void);

static int tty_ioctl_tcgets(fs_handle h, void *argp)
{
   int rc = copy_to_user(argp, &c_term, sizeof(struct termios));

   if (rc < 0)
      return -EFAULT;

   return 0;
}

static int tty_ioctl_tcsets(fs_handle h, void *argp)
{
   struct termios saved = c_term;
   int rc = copy_from_user(&c_term, argp, sizeof(struct termios));

   if (rc < 0) {
      c_term = saved;
      return -EFAULT;
   }

   tty_update_special_ctrl_handlers();
   return 0;
}

static int tty_ioctl_tiocgwinsz(fs_handle h, void *argp)
{
   struct winsize sz = {
      .ws_row = term_get_rows(get_curr_term()),
      .ws_col = term_get_cols(get_curr_term()),
      .ws_xpixel = 0,
      .ws_ypixel = 0
   };

   int rc = copy_to_user(argp, &sz, sizeof(struct winsize));

   if (rc < 0)
      return -EFAULT;

   return 0;
}

void tty_setup_for_panic(void)
{
   if (tty_kd_mode != KD_TEXT) {

      /*
       * NOTE: don't try to always fully restart the video output
       * because it might trigger a nested panic. When tty_kd_mode != KD_TEXT,
       * we have no other choice, if we wanna see something on the screen.
       *
       * TODO: investigate whether it is possible to make
       * term_restart_video_output() safer in panic scenarios.
       */
      term_restart_video_output(get_curr_term());
      tty_kd_mode = KD_TEXT;
   }
}

static int tty_ioctl_kdsetmode(fs_handle h, void *argp)
{
   uptr opt = (uptr) argp;

   if (opt == KD_TEXT) {
      term_restart_video_output(get_curr_term());
      tty_kd_mode = KD_TEXT;
      return 0;
   }

   if (opt == KD_GRAPHICS) {
      term_pause_video_output(get_curr_term());
      tty_kd_mode = KD_GRAPHICS;
      return 0;
   }

   return -EINVAL;
}

static int tty_ioctl_KDGKBMODE(fs_handle h, void *argp)
{
   int mode = K_XLATE; /* The only supported mode, at the moment */

   if (!copy_to_user(argp, &mode, sizeof(int)))
      return 0;

   return -EFAULT;
}

static int tty_ioctl_KDSKBMODE(fs_handle h, void *argp)
{
   uptr mode = (uptr) argp;

   if (mode == K_XLATE)
      return 0;  /* K_XLATE is the only supported mode, at the moment */

   return -EINVAL;
}

int tty_ioctl(fs_handle h, uptr request, void *argp)
{
   devfs_file_handle *dh = h;
   devfs_file *df = dh->devfs_file_ptr;
   tty *t = ttys[df->dev_minor];
   (void)t;

   switch (request) {

      case TCGETS:
         return tty_ioctl_tcgets(h, argp);

      case TCSETS:
         return tty_ioctl_tcsets(h, argp);

      case TCSETSW:
         // TODO: implement the correct behavior for TCSETSW
         return tty_ioctl_tcsets(h, argp);

      case TCSETSF:
         // TODO: implement the correct behavior for TCSETSF
         return tty_ioctl_tcsets(h, argp);

      case TIOCGWINSZ:
         return tty_ioctl_tiocgwinsz(h, argp);

      case KDSETMODE:
         return tty_ioctl_kdsetmode(h, argp);

      case KDGKBMODE:
         return tty_ioctl_KDGKBMODE(h, argp);

      case KDSKBMODE:
         return tty_ioctl_KDSKBMODE(h, argp);

      default:
         printk("WARNING: unknown tty_ioctl() request: %p\n", request);
         return -EINVAL;
   }
}

int tty_fcntl(fs_handle h, int cmd, uptr arg)
{
   devfs_file_handle *dh = h;
   devfs_file *df = dh->devfs_file_ptr;
   tty *t = ttys[df->dev_minor];
   (void)t;

   if (cmd == F_GETFL)
      return dh->flags;

   if (cmd == F_SETFL) {
      /*
       * TODO: check the flags in arg and fail with EINVAL in case of unknown
       * or unsupported flags. Just setting hb->flags to 'arg' will silently
       * ignore such unknown/unsupported flags and that will make hard to guess
       * why programs behave in Tilck differently than on Linux.
       */
      dh->flags = arg;
      return 0;
   }

   return -EINVAL;
}
