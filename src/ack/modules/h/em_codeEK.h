/*	@(#)em_codeEK.h	1.2	94/04/06 11:11:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* this part is generated from ../../../etc/em_table at:  Wed Mar 14 10:11:41 MET 1990 */
#define C_aar(w) CC_opcst(op_aar, w)
#define C_aar_narg() CC_opnarg(op_aar)
#define C_adf(w) CC_opcst(op_adf, w)
#define C_adf_narg() CC_opnarg(op_adf)
#define C_adi(w) CC_opcst(op_adi, w)
#define C_adi_narg() CC_opnarg(op_adi)
#define C_adp(c) CC_opcst(op_adp, c)
#define C_ads(w) CC_opcst(op_ads, w)
#define C_ads_narg() CC_opnarg(op_ads)
#define C_adu(w) CC_opcst(op_adu, w)
#define C_adu_narg() CC_opnarg(op_adu)
#define C_and(w) CC_opcst(op_and, w)
#define C_and_narg() CC_opnarg(op_and)
#define C_asp(c) CC_opcst(op_asp, c)
#define C_ass(w) CC_opcst(op_ass, w)
#define C_ass_narg() CC_opnarg(op_ass)
#define C_beq(b) CC_opilb(op_beq, b)
#define C_bge(b) CC_opilb(op_bge, b)
#define C_bgt(b) CC_opilb(op_bgt, b)
#define C_ble(b) CC_opilb(op_ble, b)
#define C_blm(c) CC_opcst(op_blm, c)
#define C_bls(w) CC_opcst(op_bls, w)
#define C_bls_narg() CC_opnarg(op_bls)
#define C_blt(b) CC_opilb(op_blt, b)
#define C_bne(b) CC_opilb(op_bne, b)
#define C_bra(b) CC_opilb(op_bra, b)
#define C_cai() CC_op(op_cai)
#define C_cal(p) CC_oppnam(op_cal, p)
#define C_cff() CC_op(op_cff)
#define C_cfi() CC_op(op_cfi)
#define C_cfu() CC_op(op_cfu)
#define C_cif() CC_op(op_cif)
#define C_cii() CC_op(op_cii)
#define C_ciu() CC_op(op_ciu)
#define C_cmf(w) CC_opcst(op_cmf, w)
#define C_cmf_narg() CC_opnarg(op_cmf)
#define C_cmi(w) CC_opcst(op_cmi, w)
#define C_cmi_narg() CC_opnarg(op_cmi)
#define C_cmp() CC_op(op_cmp)
#define C_cms(w) CC_opcst(op_cms, w)
#define C_cms_narg() CC_opnarg(op_cms)
#define C_cmu(w) CC_opcst(op_cmu, w)
#define C_cmu_narg() CC_opnarg(op_cmu)
#define C_com(w) CC_opcst(op_com, w)
#define C_com_narg() CC_opnarg(op_com)
#define C_csa(w) CC_opcst(op_csa, w)
#define C_csa_narg() CC_opnarg(op_csa)
#define C_csb(w) CC_opcst(op_csb, w)
#define C_csb_narg() CC_opnarg(op_csb)
#define C_cuf() CC_op(op_cuf)
#define C_cui() CC_op(op_cui)
#define C_cuu() CC_op(op_cuu)
#define C_dch() CC_op(op_dch)
#define C_dec() CC_op(op_dec)
#define C_dee(g) CC_opcst(op_dee, g)
#define C_dee_dnam(g, o) CC_opdnam(op_dee, g, o)
#define C_dee_dlb(g, o) CC_opdlb(op_dee, g, o)
#define C_del(c) CC_opcst(op_del, c)
#define C_dup(c) CC_opcst(op_dup, c)
#define C_dus(w) CC_opcst(op_dus, w)
#define C_dus_narg() CC_opnarg(op_dus)
#define C_dvf(w) CC_opcst(op_dvf, w)
#define C_dvf_narg() CC_opnarg(op_dvf)
#define C_dvi(w) CC_opcst(op_dvi, w)
#define C_dvi_narg() CC_opnarg(op_dvi)
#define C_dvu(w) CC_opcst(op_dvu, w)
#define C_dvu_narg() CC_opnarg(op_dvu)
#define C_exg(w) CC_opcst(op_exg, w)
#define C_exg_narg() CC_opnarg(op_exg)
#define C_fef(w) CC_opcst(op_fef, w)
#define C_fef_narg() CC_opnarg(op_fef)
#define C_fif(w) CC_opcst(op_fif, w)
#define C_fif_narg() CC_opnarg(op_fif)
#define C_fil(g) CC_opcst(op_fil, g)
#define C_fil_dnam(g, o) CC_opdnam(op_fil, g, o)
#define C_fil_dlb(g, o) CC_opdlb(op_fil, g, o)
#define C_gto(g) CC_opcst(op_gto, g)
#define C_gto_dnam(g, o) CC_opdnam(op_gto, g, o)
#define C_gto_dlb(g, o) CC_opdlb(op_gto, g, o)
#define C_inc() CC_op(op_inc)
#define C_ine(g) CC_opcst(op_ine, g)
#define C_ine_dnam(g, o) CC_opdnam(op_ine, g, o)
#define C_ine_dlb(g, o) CC_opdlb(op_ine, g, o)
#define C_inl(c) CC_opcst(op_inl, c)
#define C_inn(w) CC_opcst(op_inn, w)
#define C_inn_narg() CC_opnarg(op_inn)
#define C_ior(w) CC_opcst(op_ior, w)
#define C_ior_narg() CC_opnarg(op_ior)
#define C_lae(g) CC_opcst(op_lae, g)
#define C_lae_dnam(g, o) CC_opdnam(op_lae, g, o)
#define C_lae_dlb(g, o) CC_opdlb(op_lae, g, o)
#define C_lal(c) CC_opcst(op_lal, c)
#define C_lar(w) CC_opcst(op_lar, w)
#define C_lar_narg() CC_opnarg(op_lar)
#define C_ldc(c) CC_opcst(op_ldc, c)
#define C_lde(g) CC_opcst(op_lde, g)
#define C_lde_dnam(g, o) CC_opdnam(op_lde, g, o)
#define C_lde_dlb(g, o) CC_opdlb(op_lde, g, o)
#define C_ldf(c) CC_opcst(op_ldf, c)
#define C_ldl(c) CC_opcst(op_ldl, c)
#define C_lfr(c) CC_opcst(op_lfr, c)
#define C_lil(c) CC_opcst(op_lil, c)
#define C_lim() CC_op(op_lim)
#define C_lin(c) CC_opcst(op_lin, c)
#define C_lni() CC_op(op_lni)
#define C_loc(c) CC_opcst(op_loc, c)
#define C_loe(g) CC_opcst(op_loe, g)
#define C_loe_dnam(g, o) CC_opdnam(op_loe, g, o)
#define C_loe_dlb(g, o) CC_opdlb(op_loe, g, o)
#define C_lof(c) CC_opcst(op_lof, c)
#define C_loi(c) CC_opcst(op_loi, c)
#define C_lol(c) CC_opcst(op_lol, c)
#define C_lor(c) CC_opcst(op_lor, c)
#define C_los(w) CC_opcst(op_los, w)
#define C_los_narg() CC_opnarg(op_los)
#define C_lpb() CC_op(op_lpb)
#define C_lpi(p) CC_oppnam(op_lpi, p)
#define C_lxa(c) CC_opcst(op_lxa, c)
#define C_lxl(c) CC_opcst(op_lxl, c)
#define C_mlf(w) CC_opcst(op_mlf, w)
#define C_mlf_narg() CC_opnarg(op_mlf)
#define C_mli(w) CC_opcst(op_mli, w)
#define C_mli_narg() CC_opnarg(op_mli)
#define C_mlu(w) CC_opcst(op_mlu, w)
#define C_mlu_narg() CC_opnarg(op_mlu)
#define C_mon() CC_op(op_mon)
#define C_ngf(w) CC_opcst(op_ngf, w)
#define C_ngf_narg() CC_opnarg(op_ngf)
#define C_ngi(w) CC_opcst(op_ngi, w)
#define C_ngi_narg() CC_opnarg(op_ngi)
#define C_nop() CC_op(op_nop)
#define C_rck(w) CC_opcst(op_rck, w)
#define C_rck_narg() CC_opnarg(op_rck)
#define C_ret(c) CC_opcst(op_ret, c)
#define C_rmi(w) CC_opcst(op_rmi, w)
#define C_rmi_narg() CC_opnarg(op_rmi)
#define C_rmu(w) CC_opcst(op_rmu, w)
#define C_rmu_narg() CC_opnarg(op_rmu)
#define C_rol(w) CC_opcst(op_rol, w)
#define C_rol_narg() CC_opnarg(op_rol)
#define C_ror(w) CC_opcst(op_ror, w)
#define C_ror_narg() CC_opnarg(op_ror)
#define C_rtt() CC_op(op_rtt)
#define C_sar(w) CC_opcst(op_sar, w)
#define C_sar_narg() CC_opnarg(op_sar)
#define C_sbf(w) CC_opcst(op_sbf, w)
#define C_sbf_narg() CC_opnarg(op_sbf)
#define C_sbi(w) CC_opcst(op_sbi, w)
#define C_sbi_narg() CC_opnarg(op_sbi)
#define C_sbs(w) CC_opcst(op_sbs, w)
#define C_sbs_narg() CC_opnarg(op_sbs)
#define C_sbu(w) CC_opcst(op_sbu, w)
#define C_sbu_narg() CC_opnarg(op_sbu)
#define C_sde(g) CC_opcst(op_sde, g)
#define C_sde_dnam(g, o) CC_opdnam(op_sde, g, o)
#define C_sde_dlb(g, o) CC_opdlb(op_sde, g, o)
#define C_sdf(c) CC_opcst(op_sdf, c)
#define C_sdl(c) CC_opcst(op_sdl, c)
#define C_set(w) CC_opcst(op_set, w)
#define C_set_narg() CC_opnarg(op_set)
#define C_sig() CC_op(op_sig)
#define C_sil(c) CC_opcst(op_sil, c)
#define C_sim() CC_op(op_sim)
#define C_sli(w) CC_opcst(op_sli, w)
#define C_sli_narg() CC_opnarg(op_sli)
#define C_slu(w) CC_opcst(op_slu, w)
#define C_slu_narg() CC_opnarg(op_slu)
#define C_sri(w) CC_opcst(op_sri, w)
#define C_sri_narg() CC_opnarg(op_sri)
#define C_sru(w) CC_opcst(op_sru, w)
#define C_sru_narg() CC_opnarg(op_sru)
#define C_ste(g) CC_opcst(op_ste, g)
#define C_ste_dnam(g, o) CC_opdnam(op_ste, g, o)
#define C_ste_dlb(g, o) CC_opdlb(op_ste, g, o)
#define C_stf(c) CC_opcst(op_stf, c)
#define C_sti(c) CC_opcst(op_sti, c)
#define C_stl(c) CC_opcst(op_stl, c)
#define C_str(c) CC_opcst(op_str, c)
#define C_sts(w) CC_opcst(op_sts, w)
#define C_sts_narg() CC_opnarg(op_sts)
#define C_teq() CC_op(op_teq)
#define C_tge() CC_op(op_tge)
#define C_tgt() CC_op(op_tgt)
#define C_tle() CC_op(op_tle)
#define C_tlt() CC_op(op_tlt)
#define C_tne() CC_op(op_tne)
#define C_trp() CC_op(op_trp)
#define C_xor(w) CC_opcst(op_xor, w)
#define C_xor_narg() CC_opnarg(op_xor)
#define C_zeq(b) CC_opilb(op_zeq, b)
#define C_zer(w) CC_opcst(op_zer, w)
#define C_zer_narg() CC_opnarg(op_zer)
#define C_zge(b) CC_opilb(op_zge, b)
#define C_zgt(b) CC_opilb(op_zgt, b)
#define C_zle(b) CC_opilb(op_zle, b)
#define C_zlt(b) CC_opilb(op_zlt, b)
#define C_zne(b) CC_opilb(op_zne, b)
#define C_zre(g) CC_opcst(op_zre, g)
#define C_zre_dnam(g, o) CC_opdnam(op_zre, g, o)
#define C_zre_dlb(g, o) CC_opdlb(op_zre, g, o)
#define C_zrf(w) CC_opcst(op_zrf, w)
#define C_zrf_narg() CC_opnarg(op_zrf)
#define C_zrl(c) CC_opcst(op_zrl, c)

/* Definition of EM procedural interface: hand-written definitions

  C_open	| char *:filename	| <hand-written>
  C_busy	|	| <hand-written>
  C_close	|	| <hand-written>
  C_magic	|	| <hand-written>
*/

#define C_df_dlb(l)	CC_dfdlb(l)
#define C_df_dnam(s)	CC_dfdnam(s)
#define C_df_ilb(l)	CC_dfilb(l)

#define C_pro(s,l)	CC_pro(s, l)
#define C_pro_narg(s)	CC_pronarg(s)
#define C_end(l)	CC_end(l)
#define C_end_narg()	CC_endnarg()

#define C_exa_dnam(s)	CC_psdnam(ps_exa, s)
#define C_exa_dlb(l)	CC_psdlb(ps_exa, l)
#define C_ina_dnam(s)	CC_psdnam(ps_ina, s)
#define C_ina_dlb(l)	CC_psdlb(ps_ina, l)
#define C_exp(s)	CC_pspnam(ps_exp, s)
#define C_inp(s)	CC_pspnam(ps_inp, s)

#define C_bss_cst(n,w,i)	CC_bhcst(ps_bss, n, w, i)
#define C_hol_cst(n,w,i)	CC_bhcst(ps_hol, n, w, i)
#define C_bss_icon(n,s,sz,i)	CC_bhicon(ps_bss,n,s,sz,i)
#define C_hol_icon(n,s,sz,i)	CC_bhicon(ps_hol,n,s,sz,i)
#define C_bss_ucon(n,s,sz,i)	CC_bhucon(ps_bss,n,s,sz,i)
#define C_hol_ucon(n,s,sz,i)	CC_bhucon(ps_hol,n,s,sz,i)
#define C_bss_fcon(n,s,sz,i)	CC_bhfcon(ps_bss,n,s,sz,i)
#define C_hol_fcon(n,s,sz,i)	CC_bhfcon(ps_hol,n,s,sz,i)
#define C_bss_dnam(n,s,off,i)	CC_bhdnam(ps_bss,n,s,off,i)
#define C_hol_dnam(n,s,off,i)	CC_bhdnam(ps_hol,n,s,off,i)
#define C_bss_dlb(n,l,off,i)	CC_bhdlb(ps_bss,n,l,off,i)
#define C_hol_dlb(n,l,off,i)	CC_bhdlb(ps_hol,n,l,off,i)
#define C_bss_ilb(n,l,i)	CC_bhilb(ps_bss,n,l,i)
#define C_hol_ilb(n,l,i)	CC_bhilb(ps_hol,n,l,i)
#define C_bss_pnam(n,s,i)	CC_bhpnam(ps_bss,n,s,i)
#define C_hol_pnam(n,s,i)	CC_bhpnam(ps_hol,n,s,i)

#define C_con_cst(v)	CC_crcst(ps_con,v)
#define C_con_icon(v,s)	CC_crxcon(ps_con,sp_icon,v,s)
#define C_con_ucon(v,s)	CC_crxcon(ps_con,sp_ucon,v,s)
#define C_con_fcon(v,s)	CC_crxcon(ps_con,sp_fcon,v,s)
#define C_con_scon(v,s)	CC_crscon(ps_con,v,s)
#define C_con_dnam(v,s)	CC_crdnam(ps_con,v,s)
#define C_con_dlb(v,s)	CC_crdlb(ps_con,v,s)
#define C_con_ilb(v)	CC_crilb(ps_con,v)
#define C_con_pnam(v)	CC_crpnam(ps_con,v)

#define C_rom_cst(v)	CC_crcst(ps_rom,v)
#define C_rom_icon(v,s)	CC_crxcon(ps_rom,sp_icon,v,s)
#define C_rom_ucon(v,s)	CC_crxcon(ps_rom,sp_ucon,v,s)
#define C_rom_fcon(v,s)	CC_crxcon(ps_rom,sp_fcon,v,s)
#define C_rom_scon(v,s)	CC_crscon(ps_rom,v,s)
#define C_rom_dnam(v,s)	CC_crdnam(ps_rom,v,s)
#define C_rom_dlb(v,s)	CC_crdlb(ps_rom,v,s)
#define C_rom_ilb(v)	CC_crilb(ps_rom,v)
#define C_rom_pnam(v)	CC_crpnam(ps_rom,v)

#define C_cst(l)	CC_cst(l)
#define C_icon(v,s)	CC_icon(v,s)
#define C_ucon(v,s)	CC_ucon(v,s)
#define C_fcon(v,s)	CC_fcon(v,s)
#define C_scon(v,s)	CC_scon(v,s)
#define C_dnam(v,s)	CC_dnam(v,s)
#define C_dlb(v,s)	CC_dlb(v,s)
#define C_ilb(l)	CC_ilb(l)
#define C_pnam(s)	CC_pnam(s)

#define C_mes_begin(ms)	CC_msstart(ms)
#define C_mes_end()	CC_msend()

#define C_exc(c1,c2)	CC_exc(c1,c2)

#ifndef ps_rom
#include <em_pseu.h>
#endif

#ifndef op_lol
#include <em_mnem.h>
#endif

#ifndef sp_icon
#include <em_spec.h>
#endif
