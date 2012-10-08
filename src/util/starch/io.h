/*	@(#)io.h	1.1	96/02/27 13:16:02 */
/* Definition of the interface to bufout */

#ifdef __STDC__
void	init_wio(void);
void	init_rio(void);
int	out_block(struct ba_list *);
void	in_block(struct ba_list *);
void	rw_stats(void);
#else
void	init_wio();
void	init_rio();
int	out_block();
void	in_block();
void	rw_stats();
#endif
