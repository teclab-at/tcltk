lappend auto_path ..
package require Img
package require photoresize 0.2
package require snit

snit::widget imgviewer {

	option -size -default 300 
		
	variable sourceimg 
	variable height
	variable width
	variable zoomimg

	variable x0 0
	variable y0 0
	variable x1 0
	variable y1 0
	variable xc 0
	variable yc 0

	variable winwidth
	variable winheight
	variable dispwidth
	variable dispheight

	variable emptyimage false

	variable mag 1.0
	variable zoomlevel 0
	variable redrawmethod fit_window

	variable dragcoords {}

	component disp

	constructor {simg args} {
		set sourceimg $simg

		set zoomimg [image create photo]
		set height [image height $simg]
		set width [image width $simg]
		
		$self configurelist $args

		if {$width == 0 || $height == 0} {
			set initwidth $options(-size)
			set initheight $options(-size)
			set emptyimage true

			set width 1
			set height 1
		} else {
			if {$width > $height} {
				set initwidth $options(-size)
				set initheight [expr {entier(ceil(max(1.0, double($options(-size))*$height/$width)))}]
			} else {
				set initheight $options(-size)
				set initwidth [expr {entier(ceil(max(1.0, double($options(-size))*$width/$height)))}]
			}
		}
		
		install disp using label $win.l -image $zoomimg -width $initwidth -height $initheight -takefocus 1
		pack $disp -expand yes -fill both
		bind $disp <Configure> [mymethod redraw]
		bind $disp <Key-plus> [mymethod zoomin]
		bind $disp <Key-minus> [mymethod zoomout]

		bind $disp <ButtonPress-1> [mymethod dragstart %x %y]
		bind $disp <B1-Motion> [mymethod dragmove %x %y]
		bind $disp <ButtonRelease-1> [mymethod dragend]

		bind $disp <MouseWheel> [mymethod wheel %D %x %y]
		bind $disp <4> [mymethod wheel +1 %x %y]
		bind $disp <5> [mymethod wheel -1 %x %y]

	}

	method redraw {} {
		if {$emptyimage} return
		
		set winwidth [winfo width $win]
		set winheight [winfo height $win]

		$self $redrawmethod
	}

	method fit_window {} {
		set magx [expr {double($winwidth) / $width}]
		set magy [expr {double($winheight) / $height}]

		set mag [expr {min($magx, $magy)}]
		
		#puts "Mag: $mag"
		
		set xc [expr {$width / 2.0}]
		set yc [expr {$height / 2.0}]

		$self fit_aspect
	}

	method fit_aspect {} {
		# Input: xc, yc, mag
		# Compute source region such that the window is filled
		# centered at xc,yc
		
		if {$emptyimage} { return }
		
		set cropwidth [expr {max(1.0, $winwidth / $mag)}]
		set cropheight [expr {max(1.0, $winheight / $mag)}]


		set x0 [expr {max(0, entier(floor($xc - $cropwidth / 2.0)))}]
		set y0 [expr {max(0, entier(floor($yc - $cropheight / 2.0)))}]


		set x1 [expr {entier(ceil($x0 + $cropwidth - 1))}]
		set y1 [expr {entier(ceil($y0 + $cropheight - 1))}]

		if {$x1 >= $width} {
			set x1 [expr {$width - 1}]
			set x0 [expr {max(0, entier(floor($x1 - $cropwidth)))}]

			set dispwidth [expr {entier(ceil(($x1 - $x0 + 1) * $mag))}]
		} else {
			set dispwidth $winwidth
		}

		if {$y1 >= $height} {
			set y1 [expr {$height - 1}]
			set y0 [expr {max(0, entier(floor($y1 - $cropheight)))}]
			
			set dispheight [expr {entier(ceil(($y1 - $y0 + 1) * $mag))}]
		} else {
			set dispheight $winheight
		}	
		
		resizephoto $sourceimg $zoomimg $x0 $y0 $x1 $y1 $dispwidth $dispheight


	}

	method scrollto {xc_ yc_} {
		set xc $xc_
		set yc $yc_
		
		$self fit_aspect
	}

	method zoomin {{pos {}}} {
		$self zoomby 1.2 $pos
	}

	method zoomout {{pos {}}} {
		$self zoomby [expr {1.0/1.2}] $pos
	}

	method zoomby {fac {pos {}}} {
		set mag [expr {$mag*$fac}]
		
		if {$emptyimage} { return }

		if {$fac > 1} {
			incr zoomlevel 1
		}

		if {$fac < 1} {
			incr zoomlevel -1
		}

		if {$zoomlevel == 0} {
			set redrawmethod fit_window
		} else {
			set redrawmethod fit_aspect
		}

		if {$pos ne {}} {
			lassign $pos x y
			# zoom in/out such that the pixel under x y stays
			# in the same relative position
			set xfrac [expr {(double($x - $winwidth / 2) / $dispwidth) *  (1 - 1.0/$fac)}]
			set yfrac [expr {(double($y - $winheight / 2) / $dispheight) *  (1 - 1.0/$fac)}]

			# (xfrac, yfrac) is the relative coordinate of (x,y)
			# with the origin in the center of the magnified window

			# (nwidth, nheight) is the size of the visible part
			set nwidth [expr {($x1 - $x0 + 1)}]
			set nheight [expr {($y1 - $y0 + 1)}]

			# the center must be shifted by frac to account for the zooming
			set xc [expr {$xc + $xfrac * $nwidth }]
			set yc [expr {$yc + $yfrac * $nheight }]

		}

		$self redraw
	}

	method wheel {dist x y} {
		if {$dist > 0} {
			$self zoomin [list $x $y]
		} else {
			$self zoomout
		}
	}

	method zoom_onetoone {} {
		set mag 1.0
		set redrawmethod fit_aspect

		$self redraw
	}

	method pixtocoord {x y} {
		# convert pixel coordinates into image coords
		set xm [expr {$x/$mag + $x0}]
		set ym [expr {$y/$mag  + $y0}]
		list $xm $ym
	}

	method dragstart {x y} {
		focus $disp
		set dragcoords [list $x $y $xc $yc]
	}

	method dragmove {x y} {
		if {$dragcoords eq {}} { return }
		lassign $dragcoords xs ys xsc ysc

		set xc [expr {($xs - $x)/$mag + $xsc}]
		set yc [expr {($ys - $y)/$mag + $ysc}]

		#puts "Move to $xc $yc"
		$self fit_aspect

	}

	method dragend {} {
		set xc [expr {($x1 + $x0) / 2}]
		set yc [expr {($y1 + $y0) / 2}]
		
		set dragcoords {}
	}

	destructor {
		if {[catch {image delete $zoomimg} err]} {
			puts "Error in destructor: $err"
		}
	}
}

set harbour [image create photo -file harbour.jpg]
imgviewer .img $harbour

pack .img -expand yes -fill both




