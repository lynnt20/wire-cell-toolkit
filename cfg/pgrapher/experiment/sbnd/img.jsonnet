local wc = import "wirecell.jsonnet";
local g = import "pgraph.jsonnet";
local f = import 'pgrapher/common/funcs.jsonnet';
local params = import "pgrapher/experiment/sbnd/simparams.jsonnet"; //added Ewerton 2023-09-10
local tools_maker = import 'pgrapher/common/tools.jsonnet';
local tools = tools_maker(params);
local anodes = tools.anodes;


// added Ewerton 2023-08-23
local nanodes = std.length(tools.anodes);
local anode_iota = std.range(0, nanodes - 1);

local img = {
    // IFrame -> IFrame
    pre_proc :: function(anode, aname = "") {

    local waveform_map = {
        type: 'WaveformMap',
        name: 'wfm',
        data: {
            //filename: "microboone-charge-error.json.bz2", //commented 2023-10-17
            filename: "sbnd-charge-error.json.bz2", //added 2023-10-17
        }, uses: [],},

    local charge_err = g.pnode({
        type: 'ChargeErrorFrameEstimator',
        name: "cefe-"+aname,
        data: {
            intag: "gauss%d" % anode.data.ident,
            outtag: "gauss_error%d" % anode.data.ident,
            anode: wc.tn(anode),
            rebin: 4,  // this number should be consistent with the waveform_map choice
            fudge_factors: [2.31, 2.31, 1.1],  // fudge factors for each plane [0,1,2]
            time_limits: [12, 800],  // the unit of this is in ticks
            errors: wc.tn(waveform_map),
        },
    }, nin=1, nout=1, uses=[waveform_map, anode]),

    // added Ewerton 2023-08-23
    local chsel_pipes = 
      g.pnode({
        type: 'ChannelSelector',
        //name: 'chsel%d' % n,
        name: 'chsel%d' % anode.data.ident,
        data: {
          channels: std.range(5632 * anode.data.ident, 5632 * (anode.data.ident + 1) - 1),
          //tags: ['orig%d' % n], // traces tag //commented? Ewerton 2023-09-xx
          tags: ['gauss%d' % anode.data.ident, 'wiener%d' % anode.data.ident], // changed Ewerton 2023-09-27
        },
      }, nin=1, nout=1, uses=[anode]),

    local mag = g.pnode({
          type: 'MagnifySink',
          name: 'magimgtest%d' % anode.data.ident,
          data: {
                output_filename: "magoutput.root",
                root_file_mode: 'UPDATE',
                frames: ['gauss%d' %anode.data.ident, 'wiener%d' %anode.data.ident],
                summaries: ['wiener%d' %anode.data.ident],
                summary_operator: { ['wiener%d' % anode.data.ident]: 'set' },
                trace_has_tag: true,
                anode: wc.tn(anode),
          },
    }, nin=1, nout=1, uses=[anode]),

    local cmm_mod = g.pnode({
        type: 'CMMModifier',
        name: "cmm-mod-"+aname,
        data: {
            cm_tag: "bad",
            trace_tag: "gauss%d" % anode.data.ident,
            anode: wc.tn(anode),
            // start: 0,   // start veto ...
            // end: 9592, // end  of veto
            // ncount_cont_ch: 2,
            // cont_ch_llimit: [296, 2336+4800 ], // veto if continues bad channels
            // cont_ch_hlimit: [671, 2463+4800 ],
            // ncount_veto_ch: 1,
            // veto_ch_llimit: [3684],  // direct veto these channels
            // veto_ch_hlimit: [3699],
            // dead_ch_ncount: 10,
            // dead_ch_charge: 1000,
            // ncount_dead_ch: 2,
            // dead_ch_llimit: [2160, 2080], // veto according to the charge size for dead channels
            // dead_ch_hlimit: [2176, 2096],
            ncount_org: 1,   // organize the dead channel ranges according to these boundaries 
            org_llimit: [0], // must be ordered ...
            org_hlimit: [3400], // must be ordered ...
        },
    }, nin=1, nout=1, uses=[anode]),

    local frame_quality_tagging = g.pnode({
        type: 'FrameQualityTagging',
        name: "frame-qual-tag-"+aname,
        data: {
            trace_tag: "gauss%d" % anode.data.ident,
            anode: wc.tn(anode),
            nrebin: 4, // rebin count ...
            length_cut: 3,
            time_cut: 3,
            ch_threshold: 100,
            n_cover_cut1: 12,
            n_fire_cut1: 14,
            n_cover_cut2: 6,
            n_fire_cut2: 6,
            fire_threshold: 0.22,
            n_cover_cut3: [1200, 1200, 1800 ],
            percent_threshold: [0.25, 0.25, 0.2 ],
            threshold1: [300, 300, 360 ],
            threshold2: [150, 150, 180 ],
            min_time: 3180,
            max_time: 7870,
            flag_corr: 1,
        },
    }, nin=1, nout=1, uses=[anode]),

    local frame_masking = g.pnode({
            type: 'FrameMasking',
            name: "frame-masking-"+aname,
            data: {
                cm_tag: "bad",
                trace_tags: ['gauss%d' % anode.data.ident,'wiener%d' % anode.data.ident,], //uncommented Ewerton 2023-09-26
                //trace_tags: ['orig%d' % anode.data.ident,], //commented added Ewerton 2023-09-26
                anode: wc.tn(anode),
            },
        }, nin=1, nout=1, uses=[anode]),
        ret: g.pipeline([chsel_pipes, cmm_mod, frame_masking, charge_err], "uboone-preproc"), // mag
    }.ret,

    // A functio that sets up slicing for an APA.
    slicing :: function(anode, aname, span=4, active_planes=[0,1,2], masked_planes=[], dummy_planes=[]) {
        ret: g.pnode({
            type: "MaskSlices",
            name: "slicing-"+aname,
            data: {
                tick_span: span,
                wiener_tag: "wiener%d" % anode.data.ident,
                summary_tag: "wiener%d" % anode.data.ident,
                charge_tag: "gauss%d" % anode.data.ident,
                error_tag: "gauss_error%d" % anode.data.ident,
                anode: wc.tn(anode),
                min_tbin: 0,
                max_tbin: 3400, //we used 0 previously. changed to 3400 to check Ewerton 2023-10-25 (original=8500 PD?)
                active_planes: active_planes,
                masked_planes: masked_planes,
                dummy_planes: dummy_planes,
                //nthreshold: [1e-6, 1e-6, 1e-6],
                nthreshold: [3.6, 3.6, 3.6], //original
                //nthreshold: [2.5, 2.5, 2.5], //changed Ewerton
                // nthreshold: [0, 0, 0], //changed Ewerton
            },
        }, nin=1, nout=1, uses=[anode]),
    }.ret,

    // A function sets up tiling for an APA incuding a per-face split.
    tiling :: function(anode, aname) {
        local tilings = g.pnode({
        //local tilings = [g.pnode({
            type: "GridTiling",
            //name: "tiling-%s-face%d"%[aname, face], //commented Ewerton 2023-09-21
            name: "tiling-%s-face%d"%[aname, anode.data.ident], //added Ewerton 2023-09-21
            data: {
                anode: wc.tn(anode),
                //face: face, //commented Ewerton 2023-09-21
                face: anode.data.ident, //added Ewerton 2023-09-21
                nudge: 1e-2, //original
            }
        //}, nin=1, nout=1, uses=[anode]) for face in [0,1]], //commented Ewerton 2023-10-06
        }, nin=1, nout=1, uses=[anode]) ,//added Ewerton 2023-10-06

        //ret: tilings[0], //commented Ewerton 2023-10-06
        ret: tilings, //added Ewerton 2023-10-06
   }.ret,

    //
    multi_active_slicing_tiling :: function(anode, name, span=4) {
        local active_planes = [[0,1,2],[0,1],[1,2],[0,2],],
        local masked_planes = [[],[2],[0],[1]],
        local iota = std.range(0,std.length(active_planes)-1),
        local slicings = [$.slicing(anode, name+"_%d"%n, span, active_planes[n], masked_planes[n]) 
            for n in iota],
        local tilings = [$.tiling(anode, name+"_%d"%n)
            for n in iota],
        local multipass = [g.pipeline([slicings[n],tilings[n]]) for n in iota],
        ret: f.fanpipe("FrameFanout", multipass, "BlobSetMerge", "multi_active_slicing_tiling-%s"%anode.name),
    }.ret,

    //
    multi_masked_1view_slicing_tiling :: function(anode, name, span=500) {
        local dummy_planes = [[1,2],[0,2],[0,1]],
        local masked_planes = [[0],[1],[2]],
        local iota = std.range(0,std.length(dummy_planes)-1),
        local slicings = [$.slicing(anode, name+"_%d"%n, span,
            active_planes=[],masked_planes=masked_planes[n], dummy_planes=dummy_planes[n])
            for n in iota],
        local tilings = [$.tiling(anode, name+"_%d"%n)
            for n in iota],
        local multipass = [g.pipeline([slicings[n],tilings[n]]) for n in iota],
        ret: f.fanpipe("FrameFanout", multipass, "BlobSetMerge", "multi_masked_slicing_tiling-%s"%anode.name),
    }.ret,

    //
    multi_masked_2view_slicing_tiling :: function(anode, name, span=500) {
        local dummy_planes = [[2],[0],[1]],
        local masked_planes = [[0,1],[1,2],[0,2]],
        local iota = std.range(0,std.length(dummy_planes)-1),
        local slicings = [$.slicing(anode, name+"_%d"%n, span,
            active_planes=[],masked_planes=masked_planes[n], dummy_planes=dummy_planes[n])
            for n in iota],
        local tilings = [$.tiling(anode, name+"_%d"%n)
            for n in iota],
        local multipass = [g.pipeline([slicings[n],tilings[n]]) for n in iota],
        ret: f.fanpipe("FrameFanout", multipass, "BlobSetMerge", "multi_masked_slicing_tiling-%s"%anode.name),
    }.ret,

    local clustering_policy = "uboone", // uboone, simple

    // Just clustering
    clustering :: function(anode, aname, spans=1.0) {
        ret : g.pnode({
            type: "BlobClustering",
            name: "blobclustering-" + aname,
            data:  { spans : spans, policy: clustering_policy }
        }, nin=1, nout=1),
    }.ret,

    // in: IBlobSet out: ICluster
    solving :: function(anode, aname) {

        local bc = g.pnode({
            type: "BlobClustering",
            name: "blobclustering-" + aname,
            data:  { policy: "uboone" }
        }, nin=1, nout=1),

        local gc = g.pnode({
            type: "GlobalGeomClustering",
            name: "global-clustering-" + aname,
            data:  { policy: "uboone" }
        }, nin=1, nout=1),

        solving :: function(suffix = "1st") {
            local bg = g.pnode({
                type: "BlobGrouping",
                name: "blobgrouping-" + aname + suffix,
                data:  {
                }
            }, nin=1, nout=1),
            local cs1 = g.pnode({
                type: "ChargeSolving",
                name: "cs1-" + aname + suffix,
                data:  {
                    weighting_strategies: ["uniform"], //"uniform", "simple", "uboone"
                    solve_config: "uboone",
                    whiten: true,
                }
            }, nin=1, nout=1),
            local cs2 = g.pnode({
                type: "ChargeSolving",
                name: "cs2-" + aname + suffix,
                data:  {
                    weighting_strategies: ["uboone"], //"uniform", "simple", "uboone"
                    solve_config: "uboone",
                    whiten: true,
                }
            }, nin=1, nout=1),
            local local_clustering = g.pnode({
                type: "LocalGeomClustering",
                name: "local-clustering-" + aname + suffix,
                data:  {
                    dryrun: false,
                }
            }, nin=1, nout=1),
            // ret: g.pipeline([bg, cs1],"cs-pipe"+aname+suffix),
            ret: g.pipeline([bg, cs1, local_clustering, cs2],"cs-pipe"+aname+suffix),
        }.ret,

        global_deghosting :: function(suffix = "1st") {
            ret: g.pnode({
                type: "ProjectionDeghosting",
                name: "ProjectionDeghosting-" + aname + suffix,
                data:  {
                    dryrun: false,
                }
            }, nin=1, nout=1),
        }.ret,

        local_deghosting :: function(config_round = 1, suffix = "1st", good_blob_charge_th=300) {
            ret: g.pnode({
                type: "InSliceDeghosting",
                name: "inslice_deghosting-" + aname + suffix,
                data:  {
                    dryrun: false,
                    config_round: config_round,
                    good_blob_charge_th: good_blob_charge_th,
                }
            }, nin=1, nout=1),
        }.ret,

        local gd1 = self.global_deghosting("1st"),
        local cs1 = self.solving("1st"),
        local ld1 = self.local_deghosting(1,"1st"),

        local gd2 = self.global_deghosting("2nd"),
        local cs2 = self.solving("2nd"),
        local ld2 = self.local_deghosting(2,"2nd"),

        local cs3 = self.solving("3rd"),
        local ld3 = self.local_deghosting(3,"3rd"),

        ret: g.pipeline([bc, gd1, cs1, ld1, gd2, cs2, ld2, cs3, ld3, gc],"uboone-solving"),
        // ret: g.pipeline([bc, cs1, ld1, gc],"simple-solving"),
    }.ret,

    dump :: function(anode, aname, drift_speed) {
        local cs = g.pnode({
            type: "ClusterFileSink",
            name: "clustersink-"+aname,
            data: {
                outname: "clusters-apa-"+aname+".tar.gz",
                format: "json", // json, numpy, dummy
            }
        }, nin=1, nout=0),
        ret: cs
    }.ret,
};

function() {
    local imgpipe (anode, multi_slicing, add_dump = true) =
    if multi_slicing == "single"
    then g.pipeline([
            // img.slicing(anode, anode.name, 109, active_planes=[0,1,2], masked_planes=[],dummy_planes=[]), // 109*22*4
            // img.slicing(anode, anode.name, 1916, active_planes=[], masked_planes=[0,1],dummy_planes=[2]), // 109*22*4
            img.slicing(anode, anode.name, 4, active_planes=[0,1,2], masked_planes=[],dummy_planes=[]), // 109*22*4
            img.tiling(anode, anode.name),
            img.solving(anode, anode.name),
            // img.clustering(anode, anode.name),
            ] + if add_dump then [
            img.dump(anode, anode.name, params.lar.drift_speed),] else [])
    else if multi_slicing == "active"
    then g.pipeline([
            img.multi_active_slicing_tiling(anode, anode.name+"-ms-active", 4),
            img.solving(anode, anode.name+"-ms-active"),
            // img.clustering(anode, anode.name+"-ms-active"),
            ] + if add_dump then [
            img.dump(anode, anode.name+"-ms-active", params.lar.drift_speed),] else [])
    else if multi_slicing == "masked"
    then g.pipeline([
            img.multi_masked_2view_slicing_tiling(anode, anode.name+"-ms-masked", 500),
            img.clustering(anode, anode.name+"-ms-masked"),
            ] + if add_dump then [
            img.dump(anode, anode.name+"-ms-masked", params.lar.drift_speed),] else [])
    else {
        local st = if multi_slicing == "multi-2view"
        then img.multi_active_slicing_tiling(anode, anode.name+"-ms-active", 4)
        else g.pipeline([
            img.slicing(anode, anode.name, 4, active_planes=[0,1,2], masked_planes=[],dummy_planes=[]), // 109*22*4
            img.tiling(anode, anode.name),]),
        local mt = if multi_slicing == "multi-2view"
        then img.multi_masked_2view_slicing_tiling(anode, anode.name+"-ms-masked", 500) // 109, 1744 (total 9592)
        else img.multi_masked_1view_slicing_tiling(anode, anode.name+"-ms-masked", 500), // 109, 1744 (total 9592),
        local active_fork = g.pipeline([
            st,
            img.solving(anode, anode.name+"-ms-active"),
            ] + if add_dump then [
            img.dump(anode, anode.name+"-ms-active", params.lar.drift_speed),] else []),
        local masked_fork = g.pipeline([
            mt,
            img.clustering(anode, anode.name+"-ms-masked"),
            ] + if add_dump then [
            img.dump(anode, anode.name+"-ms-masked", params.lar.drift_speed),] else []),
        ret: g.fan.fanout("FrameFanout",[active_fork,masked_fork], "fan_active_masked-%s"%anode.name),
    }.ret,


    per_anode(anode, multi_slicing = "single", add_dump = true) :: g.pipeline([
        img.pre_proc(anode, anode.name),
        imgpipe(anode, multi_slicing, add_dump),
        ], "per_anode"),
}
