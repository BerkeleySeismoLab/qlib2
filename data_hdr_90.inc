C***********************************************************************
C*  data_hdr structure used to store parsed Mini-SEED header and	
C*  blockettes in an accessible manner.					
C*									
C*	Douglas Neuhauser						
C*	Seismographic Station						
C*	University of California, Berkeley				
C*	doug@seismo.berkeley.edu					
C*									
C***********************************************************************

C*
C* Copyright (c) 1996 The Regents of the University of California.
C* All Rights Reserved.
C* 
C* Permission to use, copy, modify, and distribute this software and its
C* documentation for educational, research and non-profit purposes,
C* without fee, and without a written agreement is hereby granted,
C* provided that the above copyright notice, this paragraph and the
C* following three paragraphs appear in all copies.
C* 
C* Permission to incorporate35z this software into commercial products may
C* be obtained from the Office of Technology Licensing, 2150 Shattuck
C* Avenue, Suite 510, Berkeley, CA  94704.
C* 
C* IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
C* FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
C* INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND
C* ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN
C* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
C* 
C* THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
C* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
C* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE
C* PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
C* CALIFORNIA HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT,
C* UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
C*

c $Id: data_hdr_90.inc,v 1.1 2018/05/17 22:52:12 doug Exp $ 

c
c Parameters for SEED data header data structures
c
	integer DH_STATION_LEN, DH_CHANNEL_LEN, DH_LOCATION_LEN, 
     1		DH_NETWORK_LEN
	parameter (	
     1		DH_STATION_LEN = 6,
     2		DH_CHANNEL_LEN = 4,
     3		DH_LOCATION_LEN	= 3,
     4		DH_NETWORK_LEN	= 3 )
c
c Date/Time data structure used for time calculations.
c
	type INT_TIME
		integer year
		integer second
		integer usec
	end type
c
c Date/Time data structure used for external interface.
c
	type EXT_TIME
		integer year
		integer doy
		integer month
		integer day
		integer hour
		integer minute
		integer second
		integer usec
	end type
c
c Data Header structure containing data from SEED Fixed Data Header
c WARNING - this assumes pointers take the same space as an integer.
c
	type DATA_HDR
		integer seq_no
		character*(DH_STATION_LEN) station_id
		character*(DH_LOCATION_LEN) location_id
		character*(DH_CHANNEL_LEN) channel_id
		character*(DH_NETWORK_LEN) network_id
		type(INT_TIME) begtime
		type(INT_TIME) endtime
		type(INT_TIME) hdrtime
		integer num_samples
		integer num_data_frames
		integer sample_rate
		integer sample_rate_mult
		integer num_blockettes
		integer num_ticks_correction
		integer first_data
		integer first_blockette
		integer pblockettes
		integer data_type
		integer blksize
		character activity_flags
		character io_flags
		character data_quality_flags
		character hdr_wordorder
		character data_wordorder
		character record_type
		character cextra(2)
		integer x0
		integer xn
		integer xm1
		integer xm2
		real rate_spsec
	end type

