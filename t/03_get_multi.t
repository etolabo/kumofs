# -*- coding: utf-8; mode: cperl -*-
use Test::Base;
use t::TestUtil;
use Data::Dumper;
use YAML::Syck;
$YAML::Syck::ImplicitTyping = 1;

plan tests => 1 * blocks;
#filters { kv => 'eval' };

run {
    my $block = shift;

    my %data = %{Load($block->data)};
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
--- data: {ka: va, kb: vb, kc: vc}

=== many
--- data: {ka: va, kb: vb, kc: vc, kd: vd, ke: ve, kf: vf, kg: vg, kh: vh, ki: vi, kj: vj, kk: vk, kl: vl, km: vm, kn: vn, ko: vo, kp: vp, kq: vq, kr: vr, ks: vs, kt: vt, ku: vu, kv: vv, kw: vw, kx: vx, ky: vy, kz: vz, kaa: vaa, kab: vab, kac: vac, kad: vad, kae: vae, kaf: vaf, kag: vag, kah: vah, kai: vai, kaj: vaj, kak: vak, kal: val, kam: vam, kan: van, kao: vao, kap: vap, kaq: vaq, kar: var, kas: vas, kat: vat, kau: vau, kav: vav, kaw: vaw, kax: vax, kay: vay, kaz: vaz, kba: vba, kbb: vbb, kbc: vbc, kbd: vbd, kbe: vbe, kbf: vbf, kbg: vbg, kbh: vbh, kbi: vbi, kbj: vbj, kbk: vbk, kbl: vbl, kbm: vbm, kbn: vbn, kbo: vbo, kbp: vbp, kbq: vbq, kbr: vbr, kbs: vbs, kbt: vbt, kbu: vbu, kbv: vbv, kbw: vbw, kbx: vbx, kby: vby, kbz: vbz, kca: vca, kcb: vcb, kcc: vcc, kcd: vcd, kce: vce, kcf: vcf, kcg: vcg, kch: vch, kci: vci, kcj: vcj, kck: vck, kcl: vcl, kcm: vcm, kcn: vcn, kco: vco, kcp: vcp, kcq: vcq, kcr: vcr, kcs: vcs, kct: vct, kcu: vcu, kcv: vcv}

