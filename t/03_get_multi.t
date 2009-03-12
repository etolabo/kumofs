# -*- coding: utf-8; mode: cperl -*-
use Test::Base;
use t::TestUtil;

=pod
== get_multi ==
  1. Managerを起動する
  2. Serverを起動する
  3. kumoctl manager attachを実行する
  4. Gatewayを起動する
  5. このテストを実行する
=cut

plan tests => 1 * blocks;
filters { data => 'yaml' };

run {
    my $block = shift;

    my %data = %{$block->data};
    my @keys = keys(%data);

    my $mc0 = create_memcache_client();
    foreach my $key (@keys) {
        $mc0->set($key, $data{$key});
    }
    undef $mc0;

    my $mc = create_memcache_client();
    my %result = %{$mc->get_multi(@keys)};
    is %result, %data, $block->name;
}

# ruby -e 'c="a"; r=[]; 100.times { r << ["k#{c}", "v#{c}"]; c.succ! }; puts "{#{r.map{|k,v|"#{k}: #{v}"}.join(", ")}}"'

__END__
=== few
--- data
--- {ka: va, kb: vb, kc: vc}

=== many
--- data
--- {ka: va, kb: vb, kc: vc, kd: vd, ke: ve, kf: vf, kg: vg, kh: vh, ki: vi, kj: vj, kk: vk, kl: vl, km: vm, kn: vn, ko: vo, kp: vp, kq: vq, kr: vr, ks: vs, kt: vt, ku: vu, kv: vv, kw: vw, kx: vx, ky: vy, kz: vz, kaa: vaa, kab: vab, kac: vac, kad: vad, kae: vae, kaf: vaf, kag: vag, kah: vah, kai: vai, kaj: vaj, kak: vak, kal: val, kam: vam, kan: van, kao: vao, kap: vap, kaq: vaq, kar: var, kas: vas, kat: vat, kau: vau, kav: vav, kaw: vaw, kax: vax, kay: vay, kaz: vaz, kba: vba, kbb: vbb, kbc: vbc, kbd: vbd, kbe: vbe, kbf: vbf, kbg: vbg, kbh: vbh, kbi: vbi, kbj: vbj, kbk: vbk, kbl: vbl, kbm: vbm, kbn: vbn, kbo: vbo, kbp: vbp, kbq: vbq, kbr: vbr, kbs: vbs, kbt: vbt, kbu: vbu, kbv: vbv, kbw: vbw, kbx: vbx, kby: vby, kbz: vbz, kca: vca, kcb: vcb, kcc: vcc, kcd: vcd, kce: vce, kcf: vcf, kcg: vcg, kch: vch, kci: vci, kcj: vcj, kck: vck, kcl: vcl, kcm: vcm, kcn: vcn, kco: vco, kcp: vcp, kcq: vcq, kcr: vcr, kcs: vcs, kct: vct, kcu: vcu, kcv: vcv}

=== manymany
--- data
--- {ka: va, kb: vb, kc: vc, kd: vd, ke: ve, kf: vf, kg: vg, kh: vh, ki: vi, kj: vj, kk: vk, kl: vl, km: vm, kn: vn, ko: vo, kp: vp, kq: vq, kr: vr, ks: vs, kt: vt, ku: vu, kv: vv, kw: vw, kx: vx, ky: vy, kz: vz, kaa: vaa, kab: vab, kac: vac, kad: vad, kae: vae, kaf: vaf, kag: vag, kah: vah, kai: vai, kaj: vaj, kak: vak, kal: val, kam: vam, kan: van, kao: vao, kap: vap, kaq: vaq, kar: var, kas: vas, kat: vat, kau: vau, kav: vav, kaw: vaw, kax: vax, kay: vay, kaz: vaz, kba: vba, kbb: vbb, kbc: vbc, kbd: vbd, kbe: vbe, kbf: vbf, kbg: vbg, kbh: vbh, kbi: vbi, kbj: vbj, kbk: vbk, kbl: vbl, kbm: vbm, kbn: vbn, kbo: vbo, kbp: vbp, kbq: vbq, kbr: vbr, kbs: vbs, kbt: vbt, kbu: vbu, kbv: vbv, kbw: vbw, kbx: vbx, kby: vby, kbz: vbz, kca: vca, kcb: vcb, kcc: vcc, kcd: vcd, kce: vce, kcf: vcf, kcg: vcg, kch: vch, kci: vci, kcj: vcj, kck: vck, kcl: vcl, kcm: vcm, kcn: vcn, kco: vco, kcp: vcp, kcq: vcq, kcr: vcr, kcs: vcs, kct: vct, kcu: vcu, kcv: vcv, kcw: vcw, kcx: vcx, kcy: vcy, kcz: vcz, kda: vda, kdb: vdb, kdc: vdc, kdd: vdd, kde: vde, kdf: vdf, kdg: vdg, kdh: vdh, kdi: vdi, kdj: vdj, kdk: vdk, kdl: vdl, kdm: vdm, kdn: vdn, kdo: vdo, kdp: vdp, kdq: vdq, kdr: vdr, kds: vds, kdt: vdt, kdu: vdu, kdv: vdv, kdw: vdw, kdx: vdx, kdy: vdy, kdz: vdz, kea: vea, keb: veb, kec: vec, ked: ved, kee: vee, kef: vef, keg: veg, keh: veh, kei: vei, kej: vej, kek: vek, kel: vel, kem: vem, ken: ven, keo: veo, kep: vep, keq: veq, ker: ver, kes: ves, ket: vet, keu: veu, kev: vev, kew: vew, kex: vex, key: vey, kez: vez, kfa: vfa, kfb: vfb, kfc: vfc, kfd: vfd, kfe: vfe, kff: vff, kfg: vfg, kfh: vfh, kfi: vfi, kfj: vfj, kfk: vfk, kfl: vfl, kfm: vfm, kfn: vfn, kfo: vfo, kfp: vfp, kfq: vfq, kfr: vfr, kfs: vfs, kft: vft, kfu: vfu, kfv: vfv, kfw: vfw, kfx: vfx, kfy: vfy, kfz: vfz, kga: vga, kgb: vgb, kgc: vgc, kgd: vgd, kge: vge, kgf: vgf, kgg: vgg, kgh: vgh, kgi: vgi, kgj: vgj, kgk: vgk, kgl: vgl, kgm: vgm, kgn: vgn, kgo: vgo, kgp: vgp, kgq: vgq, kgr: vgr}
