/************************************************************************/
/*  Routines for packing MiniSEED records.				*/
/*									*/
/*	Douglas Neuhauser						*/
/*	Seismological Laboratory					*/
/*	University of California, Berkeley				*/
/*	doug@seismo.berkeley.edu					*/
/*									*/
/************************************************************************/

/*
 * Copyright (c) 1996-2018 The Regents of the University of California.
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research and non-profit purposes,
 * without fee, and without a written agreement is hereby granted,
 * provided that the above copyright notice, this paragraph and the
 * following three paragraphs appear in all copies.
 * 
 * Permission to incorporate this software into commercial products may
 * be obtained from the Office of Technology Licensing, 2150 Shattuck
 * Avenue, Suite 510, Berkeley, CA  94704.
 * 
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 * INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND
 * ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
 * CALIFORNIA HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT,
 * UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "qdefines.h"
#include "msdatatypes.h"
#include "timedef.h"
#include "qsteim.h"
#include "sdr.h"
#include "data_hdr.h"
#include "qtime.h"
#include "qutils.h"
#include "sdr_utils.h"
#include "ms_pack2.h"
#include "pack.h"

#define	MALLOC_INCREMENT    1000		/* # of Mini-SEED blocks to alloc*/

/************************************************************************/
/*  ms_pack2_data:							*/
/*	Pack data into miniSEED records in specified data format.	*/
/*									*/
/*	If *pp_ms is NULL, space for the miniSEED records will be	*/
/*	allocated by packing routines, and should be freed by the	*/
/*	calling routine.  Otherwise, the packing routines will use	*/
/*	the space pointed to by *pp_ms.					*/
/*	NOTE: Steim compression routines will use values in xm1, xm2	*/
/*	to initialize compressor.					*/
/*									*/
/*	Return:								*/
/*	    0 on success.						*/
/*	    negative QLIB2 error code on error.				*/
/*	Update x0, xn, xm1, xm2 in DATA_HDR.				*/
/*	x0 = first data value from THIS data buffer.			*/
/*	xn = last data value from THIS data buffer.			*/
/*	xm1 = last data value from THIS data buffer.			*/
/*	    Assign this value to xm1 in data_hdr for NEXT call to	*/
/*	    ms_pack for consecutive data to maintain compressor state.	*/
/*	xm2 = next to last data value from THIS data buffer.		*/
/*	    Assign this value to xm2 in data_hdr for NEXT call to	*/
/*	    ms_pack for consecutive data to maintain compressor state.	*/
/************************************************************************/
int ms_pack2_data
   (DATA_HDR	*hdr,		/* ptr to initial data hdr.		*/
    BS		*init_bs,	/* ptr to onetime blockettes.		*/
    int		num_samples,	/* number of data samples.		*/
    void	*data,		/* ptr to data buffer.			*/
    int		*n_blocks,	/* # miniSEED blocks (returned).	*/
    int		*n_samples,	/* # data samples packed (returned).	*/
    char	**pp_ms,	/* ptr **miniSEED (returned).		*/
    int		ms_len,		/* miniSEED buffer len (if supplied).	*/
    char	*p_errmsg)	/* ptr to error msg buffer.		*/

{
    int status = 0;		/* status from data packing routine.	*/

    switch (hdr->data_type) {
      case STEIM1:
      case STEIM2:
	status = ms_pack2_steim (hdr, init_bs, (int*)data, NULL, num_samples, 
				n_blocks, n_samples, pp_ms, ms_len, p_errmsg);
	break;
      case INT_32:
      case INT_24:
      case INT_16:
	status = ms_pack2_int (hdr, init_bs, (int*)data, num_samples, 
			     n_blocks, n_samples, pp_ms, ms_len, p_errmsg);
	break;
      case IEEE_FP_SP:
	status = ms_pack2_float (hdr, init_bs, (float*)data, num_samples, 
			     n_blocks, n_samples, pp_ms, ms_len, p_errmsg);
	break;
      case IEEE_FP_DP:
	status = ms_pack2_double (hdr, init_bs, (double*)data, num_samples, 
			     n_blocks, n_samples, pp_ms, ms_len, p_errmsg);
	break;
      case UNKNOWN_DATATYPE:
	/* Unknown datatype is valid if sample_rate is 0. */
	if (hdr->sample_rate == 0) {
	    status = ms_pack2_text (hdr, init_bs, (char *)data, num_samples,
			     n_blocks, n_samples, pp_ms, ms_len, p_errmsg);

	    break;
	}
      default:
	if (p_errmsg) sprintf (p_errmsg, "ms_pack2_data: Unimplemented data format: %d\n",
			     hdr->data_type);
	else fprintf (stderr, "ms_pack2_data: Unimplemented data format: %d\n",
			     hdr->data_type);
	*n_samples = 0;
	*n_blocks = 0;
	status = MS_ERROR;
	break;
    }
    return (status);
}

/************************************************************************/
/*  ms_pack2_update_hdr:							*/
/*	Update header information from previous ms_pack2_data call.	*/
/*	Update hdr time, num_samples, and info required for compressor.	*/
/************************************************************************/
int ms_pack2_update_hdr
   (DATA_HDR	*hdr,		/* ptr to data hdr to update.		*/
    int		num_records,	/* number of mseed records just packed.	*/
    int		num_samples,	/* number of samples just packed.	*/
    int		*data)		/* data buffer used for last ms_pack.	*/
{
    BS		    *bs;	/* ptr to blockette structure.		*/

    int seconds, usecs;	
    /* Update the time in the header.					*/
    /* Use the actual sample rate in the blockette 100 if it exists.	*/
    /* Otherwise, use the nominal rate in the data_hdr.			*/
    if ((bs=find_blockette(hdr,100))) {
	double actual_rate, dusecs;
        BLOCKETTE_100 *b = (BLOCKETTE_100 *) bs->pb;
	actual_rate = b->actual_rate;
	dusecs = ((double)num_samples/actual_rate)*USECS_PER_SEC;
	hdr->begtime = add_dtime (hdr->begtime,  dusecs);
	hdr->hdrtime = add_dtime (hdr->hdrtime,  dusecs);
    }
    else {
	time_interval2 (num_samples, hdr->sample_rate, hdr->sample_rate_mult,
			&seconds, &usecs);
	hdr->begtime = add_time (hdr->begtime, seconds, usecs);
	hdr->hdrtime = add_time (hdr->hdrtime, seconds, usecs);
    }
    hdr->num_samples -= num_samples;
    if (hdr->data_type == STEIM1 || hdr->data_type == STEIM2 || hdr->data_type == INT_32
	|| hdr->data_type == INT_24 || hdr->data_type == INT_16 ) {
	if (num_samples <= 0) {
	    /* No change from input. */
	}
	else if (num_samples == 1) {
	    /* Packed only 1 data sample. */
	    hdr->xm2 = hdr->xm1;
	    hdr->xm1 = data[0];
	    hdr->x0  = data[0];
	    hdr->xn  = data[0];
	}
	else {
	    /* Packed multiple data samples */
	    hdr->x0  = data[0];
	    hdr->xn  = data[num_samples-1];
	    hdr->xm1 = data[num_samples-1];
	    hdr->xm2 = data[num_samples-2];
	}
    }
    else {
	hdr->x0 = hdr->xn = hdr->xm1 = hdr->xm2 = 0;
    }
    hdr->seq_no += num_records;
    return (0);
}

/************************************************************************/
/*  ms_pack2_update_return_hdr:						*/
/*	Update header information from data packing.			*/
/*	Update info required for subsequent data compressor.		*/
/************************************************************************/
int ms_pack2_update_return_hdr
   (DATA_HDR	*hdr,		/* ptr to data hdr to update.		*/
    int		num_records,	/* number of mseed records just packed.	*/
    int		num_samples,	/* number of samples just packed.	*/
    int		*data)		/* data buffer used for this packing.	*/
{
    if (hdr->data_type == STEIM1 || hdr->data_type == STEIM2 || hdr->data_type == INT_32
	|| hdr->data_type == INT_24 || hdr->data_type == INT_16 ) {
	if (num_samples <= 0) {
	    /* No change from input. */
	}
	else if (num_samples == 1) {
	    /* Packed only 1 data sample. */
	    hdr->xm2 = hdr->xm1;
	    hdr->xm1 = data[0];
	    hdr->x0  = data[0];
	    hdr->xn  = data[0];
	}
	else {
	    /* Packed multiple data samples */
	    hdr->x0  = data[0];
	    hdr->xn  = data[num_samples-1];
	    hdr->xm1 = data[num_samples-1];
	    hdr->xm2 = data[num_samples-2];
	}
    }
    else {
	hdr->x0 = hdr->xn = hdr->xm1 = hdr->xm2 = 0;
    }
    return (0);
}

/************************************************************************/
/*  init_miniseed_hdr:							*/
/*	Initialize a miniSEED header.					*/
/*	Return 0 on success, QLIB2 error code on error.			*/
/************************************************************************/
int init_miniseed_hdr
   (SDR_HDR	*sh,		/* ptr to space for miniSEED data hdr.	*/
    DATA_HDR	*hdr,		/* initial DATA_HDR for miniSEED record.*/
    BS		*extra_bs)	/* ptr to block-specific blockettes.	*/
{
    int status = 0;

    /* Ensure that we have a blockette 1000, required by miniSEED.	*/
    if (add_required_miniseed_blockettes (hdr) != 0) {
	return (MS_ERROR);
    }

    status = init_sdr_hdr (sh, hdr, extra_bs);
    /* Return status our miniSEED header creation.			*/
    return (status);
}

/************************************************************************/
/*  update_miniseed_hdr:						*/
/*	Update a previously constructed miniSEED header.		*/
/*	Return 0 on success, negative QLIB2 error code on error.	*/
/************************************************************************/
int update_miniseed_hdr
   (SDR_HDR	*sh,		/* ptr to space for miniSEED data hdr.	*/
    DATA_HDR	*hdr)		/* initial DATA_HDR for miniSEED record.*/
{
    update_sdr_hdr (sh, hdr);
    return (0);
}

/************************************************************************/
/*  ms_pack2_steim:							*/
/*	Pack data into Mini-SEED records in STEIM1 or STEIM2 format.	*/
/*									*/
/*	If *pp_ms is NULL, space for the Mini-SEED records will be	*/
/*	allocated by packing routines, and should be freed by the	*/
/*	calling routine.  Otherwise, the packing routines will use	*/
/*	the space pointed to by *pp_ms.					*/
/*									*/
/*	Return:								*/
/*	    0 on success.						*/
/*	    negative QLIB2 error code on error.				*/
/************************************************************************/
int ms_pack2_steim 
   (DATA_HDR	*hdr0,		/* ptr to initial data hdr.		*/
    BS		*init_bs,	/* ptr to onetime blockettes.		*/
    int		*data,		/* ptr to data buffer.			*/
    int		*diff,		/* ptr to diff buffer (optional)	*/
    int		num_samples,	/* number of data samples.		*/
    int		*n_blocks,	/* # miniSEED blocks (returned).	*/
    int		*n_samples,	/* # data samples packed (returned).	*/
    char	**pp_ms,	/* ptr **miniSEED (returned).		*/
    int		ms_len,		/* miniSEED buffer len (if supplied).	*/
    char	*p_errmsg)	/* ptr to error msg buffer.		*/
{
    DATA_HDR *hdr;		/* data_hdr used for writing Mini-SEED	*/
    char *p_ms;			/* ptr to current Mini-SEED block.	*/
    SDF *p_sdf;			/* ptr to STEIM data frame.		*/
    char errmsg[256];		/* error msg buffer.			*/
    unsigned char *minbits;	/* min # of bits required to pack data.	*/
    int free_diff = 0;		/* flag to remind whether we free diff.	*/
    int ipt;			/* index of data to pack.		*/
    int nblks_malloced;		/* # Mini-SEED output blocks malloced.	*/
    int num_blocks;		/* # Mini-SEED block created.		*/
    int samples_remaining;	/* # samples left to cvt to Mini-seed.	*/
    int frames_per_block;	/* # of steim compressed framed per blk.*/
    int	nframes;		/* # of steim frames in Mini-SEED block.*/
    int nsamples;		/* # of samples in Mini-SEED block.	*/
    int pad;			/* flag to indicate padding of frames.	*/
    int status = 0;		/* status from data packing routine.	*/
    int i;			/* loop indices.			*/
    int blksize = hdr0->blksize;/* output blksize.			*/

    /* Initialization.							*/
    *n_blocks = 0;
    minbits = NULL;

    /* Check for invalid arguments.					*/
    if (num_samples <= 0) return(MS_ERROR);
    if (blksize < 128 || 
	(blksize != (int)pow(2.0,floor(log2((double)blksize))))) {
	sprintf (errmsg, "Warning: invalid blksize: %d\n", blksize);
	if (p_errmsg) strcpy(p_errmsg, errmsg);
	else fprintf (stderr, "%s", errmsg);
	return (MS_ERROR);
    }

    /* If no diff buffer provided, create one and compute differences.	*/
    if (diff == NULL) {
	if ((diff = (int *)malloc(num_samples * sizeof(int))) == NULL) {
	    sprintf (errmsg, "Error mallocing diff buffer\n");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else fprintf (stderr, "%s", errmsg);
	    return (QLIB2_MALLOC_ERROR);
	}
	free_diff = 1;
	diff[0] = data[0] - hdr0->xm1;
	for (i=1; i<num_samples; i++) {
	    int64_t diff64;
	    /* Check for possible 32-bit int overflow/underflow.    */
	    /* If found, set diff to max/min 32-bit int value.	    */
	    /* This will raise a MS_COMPRESS error in the STEIM	    */
	    /* packing functions.				    */
	    diff64 = (int64_t)data[i] - (int64_t)data[i-1];
	    if (diff64 > INT_MAX) diff64 = INT_MAX;
	    if (diff64 < INT_MIN) diff64 = INT_MIN;
	    diff[i] = (int)diff64;
	}
    }

    /* If *pp_ms != NULL, assume that the caller is providing sufficient*/
    /* memory to hold all of the resulting Mini-SEED records.		*/
    /* If it is NULL, we allocate the space, and set it to point to the	*/
    /* allocated Mini-SEED records.					*/
    /* If we allocated the space for the Mini-SEED, the caller is	*/
    /* responsible for freeing the space.				*/
    if (*pp_ms) nblks_malloced = -1;
    else nblks_malloced = 0;

    /* Create a copy of the initial data_hdr for our use.		*/
    /* We will update this each time we create a Mini-SEED block.	*/
    hdr = dup_data_hdr (hdr0);
    if (hdr == NULL) {
	if (free_diff) free(diff);
	return (MS_ERROR);
    }

    /* Start compressor.						*/
    num_blocks = 0;
    samples_remaining = num_samples;
    ipt = 0;
    pad = 1;

    while (samples_remaining > 0 && status == 0) {
	/* Check for available space.					*/
	/* Allocate more space for Mini-SEED blocks if necessary.	*/
	if (nblks_malloced < 0) {
	    if (ms_len < blksize) {
		*n_blocks = num_blocks;
		if(free_diff) free( (char *) diff);
		free_data_hdr(hdr);
		return (num_samples - samples_remaining);
	    }
	    ms_len -= blksize;
	}
	if (nblks_malloced >= 0 && num_blocks == nblks_malloced) {
	    *pp_ms = (*pp_ms == NULL) ?
		(char *)malloc((nblks_malloced+MALLOC_INCREMENT)*blksize) :
		(char *)realloc(*pp_ms,(nblks_malloced+MALLOC_INCREMENT)*blksize);
	    if (*pp_ms == NULL) {
		sprintf (errmsg, "Error mallocing Mini-SEED buffer\n");
		if (p_errmsg) strcpy(p_errmsg, errmsg);
		else fprintf (stderr, "%s", errmsg);	    
		if (free_diff) free ((char *)diff);
		free_data_hdr (hdr);
		return (QLIB2_MALLOC_ERROR);
	    }
	    nblks_malloced += MALLOC_INCREMENT;
	}

	/* Initialize the next fixed data header.			*/
	p_ms = *pp_ms + (num_blocks * blksize);
	if (init_miniseed_hdr ((SDR_HDR *)p_ms, hdr, init_bs) < 0) {
	    sprintf (errmsg, "Error: initializing MiniSEED header");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else fprintf (stderr, "%s", errmsg);	    
	    if (free_diff) free ((char *)diff);
	    free_data_hdr (hdr);
	    if (nblks_malloced > 0) free(*pp_ms);
	    return (MS_ERROR);
	}
	    
	init_bs = NULL;
	frames_per_block = (blksize-hdr->first_data) / 64;
	p_sdf = (SDF *)(p_ms + hdr->first_data);

	/* Pack data into the next Mini-SEED block.			*/
	nframes = nsamples = 0;
	switch (hdr->data_type) {
	  case STEIM1:
	    status = pack_steim1 (p_sdf, &data[ipt], &diff[ipt], 
				  samples_remaining, frames_per_block, 
				  pad, hdr->data_wordorder, &nframes, &nsamples);
	    break;
	  case STEIM2:
	    status = pack_steim2 (p_sdf, &data[ipt], &diff[ipt], 
				  samples_remaining, frames_per_block, 
				  pad, hdr->data_wordorder, &nframes, &nsamples);
	    break;
	  default:
	    sprintf (errmsg, "Error: invalid format %d for ms_pack_steim\n",
		     hdr->data_type);
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else fprintf (stderr, "%s", errmsg);	    
	    fflush (stderr);
	    if (QLIB2_CLASSIC) exit (1);
	    if (free_diff) free ((char *)diff);
	    free_data_hdr (hdr);
 	    if (nblks_malloced > 0) free(*pp_ms);
	    return (MS_ERROR);
	    break;
	}

	if (status != 0) {
	    sprintf (errmsg, "Error packing %s data\n",
		(hdr->data_type == STEIM1) ? "STEIM1" : "STEIM2");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else {
		fprintf (stderr, "%s", errmsg);
		fflush (stderr);
	    }
	}

	/* End of data or Mini-SEED block is full or packing error.	*/
	/* Update Mini-SEED header with:				*/
	/*	final sample count.					*/
	/* Update hdr for the next record.				*/
	if (nsamples > 0) {
	    hdr->num_samples = nsamples;
	    update_miniseed_hdr ((SDR_HDR *)p_ms, hdr);
	    ms_pack2_update_hdr (hdr, 1, nsamples, &data[ipt]);
	    ipt += nsamples;
	    samples_remaining -= nsamples;
	    ++num_blocks;
	    hdr->num_samples = 0;
	}
    }

    /* Cleanup.								*/
    free ((char *)minbits);
    free_data_hdr (hdr);
    if (free_diff) free ((char *)diff);
    ms_pack2_update_return_hdr (hdr0, num_blocks, num_samples, data);
    *n_blocks = num_blocks;
    *n_samples = num_samples - samples_remaining;
    return(status);
}

/************************************************************************/
/*  ms_pack2_int:							*/
/*	Pack data into miniSEED records in INT_32, INT_24, or INT_16	*/
/*	format.								*/
/*									*/
/*	If *pp_ms is NULL, space for the miniSEED records will be	*/
/*	allocated by packing routines, and should be freed by the	*/
/*	calling routine.  Otherwise, the packing routines will use	*/
/*	the space pointed to by *pp_ms.					*/
/*									*/
/*	Return:								*/
/*	    # samples packed on success.				*/
/*	    negative QLIB2 error code on error.				*/
/************************************************************************/
int ms_pack2_int 
   (DATA_HDR	*hdr0,		/* ptr to initial data hdr.		*/
    BS		*init_bs,	/* ptr to onetime blockettes.		*/
    int		*data,		/* ptr to data buffer.			*/
    int		num_samples,	/* number of data samples.		*/
    int		*n_blocks,	/* # miniSEED blocks (returned).	*/
    int		*n_samples,	/* # data samples packed (returned).	*/
    char	**pp_ms,	/* ptr **miniSEED (returned).		*/
    int		ms_len,		/* miniSEED buffer len (if supplied).	*/
    char	*p_errmsg)	/* ptr to error msg buffer.		*/
{
    DATA_HDR *hdr;		/* data header used for writing miniSEED*/
    char *p_ms;			/* ptr to current miniSEED block.	*/
    void *p_packed;		/* ptr to packed output data.		*/
    char errmsg[256];		/* error msg buffer.			*/
    int ipt;			/* index of data to pack.		*/
    int nblks_malloced;		/* # of miniSEED output blocks malloced.*/
    int num_blocks;		/* # of miniSEED block created.		*/
    int samples_remaining;	/* # samples left to cvt to miniseed.	*/
    int nsamples;		/* # of samples in miniSEED block.	*/
    int max_bytes;		/* max # of data bytes in record.	*/
    int nbytes;			/* # of bytes packed into record.	*/
    int pad;			/* flag to indicate padding of frames.	*/
    int blksize = hdr0->blksize;/* output blksize.			*/
    int status = 0;		/* status from data packing routine.	*/

    /* Initialization.							*/
    *n_blocks = 0;

    /* Check for invalid arguments.					*/
    if (num_samples <= 0) return(MS_ERROR);
    if (blksize < 128 ||
	(blksize != (int)pow(2.0,floor(log2((double)blksize))))) {
	sprintf (errmsg, "Warning: invalid blksize: %d\n", blksize);
	if (p_errmsg) strcpy(p_errmsg, errmsg);
	else fprintf (stderr, "%s", errmsg);
	return (MS_ERROR);
    }

    /* If *pp_ms != NULL, assume that the caller is providing sufficient*/
    /* memory to hold all of the resulting miniSEED records.		*/
    /* If it is NULL, we allocate the space, and set it to point to the	*/
    /* allocated miniSEED records.					*/
    /* If we allocated the space for the miniSEED, the caller is	*/
    /* responsible for freeing the space.				*/
    if (*pp_ms) nblks_malloced = -1;
    else nblks_malloced = 0;

    /* Create a copy of the initial data_hdr for our use.		*/
    /* We will update this each time we create a miniSEED block.	*/
    hdr = dup_data_hdr (hdr0);
    if (hdr == NULL) {
	return (MS_ERROR);
    }

    /* Start compressor.						*/
    num_blocks = 0;
    samples_remaining = num_samples;
    ipt = 0;
    pad = 1;

    while (samples_remaining > 0 && status == 0) {
	/* Check for available space.					*/
	/* Allocate more space for Mini-SEED blocks if necessary.	*/
	if (nblks_malloced < 0) {
	    if (ms_len < blksize) {
		*n_blocks = num_blocks;
		free_data_hdr (hdr);
		return (num_samples - samples_remaining);
	    }
	    ms_len -= blksize;
	}
	if (nblks_malloced >= 0 && num_blocks == nblks_malloced) {
	    *pp_ms = (*pp_ms == NULL) ?
		(char *)malloc((nblks_malloced+MALLOC_INCREMENT)*blksize) :
		(char *)realloc(*pp_ms,(nblks_malloced+MALLOC_INCREMENT)*blksize);
	    if (*pp_ms == NULL) {
		sprintf (errmsg, "Error mallocing miniSEED buffer\n");
		if (p_errmsg) strcpy(p_errmsg, errmsg);
		else fprintf (stderr, "%s", errmsg);	    
		free_data_hdr (hdr);
		return (QLIB2_MALLOC_ERROR);
	    }
	    nblks_malloced += MALLOC_INCREMENT;
	}

	/* Initialize the next fixed data header.			*/
	p_ms = *pp_ms + (num_blocks * blksize);
	if (init_miniseed_hdr ((SDR_HDR *)p_ms, hdr, init_bs) < 0) {
	    sprintf (errmsg, "Error: initializing MiniSEED header");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else fprintf (stderr, "%s", errmsg);	    
	    free_data_hdr (hdr);
	    if (nblks_malloced > 0) free(*pp_ms);
	    return (MS_ERROR);
	}
	    
	init_bs = NULL;
	p_packed = (void *)(p_ms + hdr->first_data);
	max_bytes = blksize - hdr->first_data;

	/* Pack the rest of the miniSEED record with data.		*/
	switch (hdr->data_type) {
	  case INT_32:
	    status = pack_int_32 ((int *)p_packed, &data[ipt], samples_remaining, 
			 max_bytes, pad, hdr->data_wordorder, &nbytes, &nsamples);
	    break;
	  case INT_24:
	    status = pack_int_24 ((unsigned char *)p_packed, &data[ipt], samples_remaining, 
			 max_bytes, pad, hdr->data_wordorder, &nbytes, &nsamples);
	    break;
	  case INT_16:
	    status = pack_int_16 ((short int *)p_packed, &data[ipt], samples_remaining, 
			 max_bytes, pad, hdr->data_wordorder, &nbytes, &nsamples);
	    break;
	}

	if (status != 0) {
	    sprintf (errmsg, "Error packing %s data\n",
		(hdr->data_type == STEIM1) ? "STEIM1" : "STEIM2");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else {
		fprintf (stderr, "%s", errmsg);
		fflush (stderr);
	    }
	}

	/* End of data or Mini-SEED block is full or packing error.	*/
	/* Update Mini-SEED header with:				*/
	/*	final sample count.					*/
	/* Update hdr for the next record.				*/
	hdr->num_samples = nsamples;
	update_miniseed_hdr ((SDR_HDR *)p_ms, hdr);
	ms_pack2_update_hdr (hdr, 1, nsamples, &data[ipt]);
	ipt += nsamples;
	samples_remaining -= nsamples;
	++num_blocks;
	hdr->num_samples = 0;
    }

    /* Cleanup.								*/
    free_data_hdr (hdr);
    ms_pack2_update_return_hdr (hdr0, num_blocks, num_samples, data);
    *n_blocks = num_blocks;
    *n_samples = num_samples - samples_remaining;
    return(status);
}

/************************************************************************/
/*  ms_pack2_float:							*/
/*	Pack data into miniSEED records in IEEE_FP_SP format    	*/
/*									*/
/*	If *pp_ms is NULL, space for the miniSEED records will be	*/
/*	allocated by packing routines, and should be freed by the	*/
/*	calling routine.  Otherwise, the packing routines will use	*/
/*	the space pointed to by *pp_ms.					*/
/*									*/
/*	Return:								*/
/*	    # samples packed on success.				*/
/*	    negative QLIB2 error code on error.				*/
/************************************************************************/
int ms_pack2_float 
   (DATA_HDR	*hdr0,		/* ptr to initial data hdr.		*/
    BS		*init_bs,	/* ptr to onetime blockettes.		*/
    float	*data,		/* ptr to data buffer.			*/
    int		num_samples,	/* number of data samples.		*/
    int		*n_blocks,	/* # miniSEED blocks (returned).	*/
    int		*n_samples,	/* # data samples packed (returned).	*/
    char	**pp_ms,	/* ptr **miniSEED (returned).		*/
    int		ms_len,		/* miniSEED buffer len (if supplied).	*/
    char	*p_errmsg)	/* ptr to error msg buffer.		*/
{
    DATA_HDR *hdr;		/* data header used for writing miniSEED*/
    char *p_ms;			/* ptr to current miniSEED block.	*/
    void *p_packed;		/* ptr to packed output data.		*/
    char errmsg[256];		/* error msg buffer.			*/
    int ipt;			/* index of data to pack.		*/
    int nblks_malloced;		/* # of miniSEED output blocks malloced.*/
    int num_blocks;		/* # of miniSEED block created.		*/
    int samples_remaining;	/* # samples left to cvt to miniseed.	*/
    int nsamples;		/* # of samples in miniSEED block.	*/
    int max_bytes;		/* max # of data bytes in record.	*/
    int nbytes;			/* # of bytes packed into record.	*/
    int pad;			/* flag to indicate padding of frames.	*/
    int blksize = hdr0->blksize;/* output blksize.			*/
    int status = 0;		/* status from data packing routine.	*/

    /* Initialization.							*/
    *n_blocks = 0;

    /* Check for invalid arguments.					*/
    if (num_samples <= 0) return(MS_ERROR);
    if (blksize < 128 ||
	(blksize != (int)pow(2.0,floor(log2((double)blksize))))) {
	sprintf (errmsg, "Warning: invalid blksize: %d\n", blksize);
	if (p_errmsg) strcpy(p_errmsg, errmsg);
	else fprintf (stderr, "%s", errmsg);
	return (MS_ERROR);
    }

    /* If *pp_ms != NULL, assume that the caller is providing sufficient*/
    /* memory to hold all of the resulting miniSEED records.		*/
    /* If it is NULL, we allocate the space, and set it to point to the	*/
    /* allocated miniSEED records.					*/
    /* If we allocated the space for the miniSEED, the caller is	*/
    /* responsible for freeing the space.				*/
    if (*pp_ms) nblks_malloced = -1;
    else nblks_malloced = 0;

    /* Create a copy of the initial data_hdr for our use.		*/
    /* We will update this each time we create a miniSEED block.	*/
    hdr = dup_data_hdr (hdr0);
    if (hdr == NULL) {
	return (MS_ERROR);
    }

    /* Start compressor.						*/
    num_blocks = 0;
    samples_remaining = num_samples;
    ipt = 0;
    pad = 1;

    while (samples_remaining > 0 && status == 0) {
	/* Check for available space.					*/
	/* Allocate more space for Mini-SEED blocks if necessary.	*/
	if (nblks_malloced < 0) {
	    if (ms_len < blksize) {
		*n_blocks = num_blocks;
		free_data_hdr (hdr);
		return (num_samples - samples_remaining);
	    }
	    ms_len -= blksize;
	}
	if (nblks_malloced >= 0 && num_blocks == nblks_malloced) {
	    if(*pp_ms == NULL) {
	      *pp_ms = (char *)malloc((nblks_malloced+MALLOC_INCREMENT)*blksize);
	    } else {
	      *pp_ms = (char *)realloc(*pp_ms,(nblks_malloced+MALLOC_INCREMENT)*blksize);
	    }
	    if (*pp_ms == NULL) {
		sprintf (errmsg, "Error mallocing miniSEED buffer\n");
		if (p_errmsg) strcpy(p_errmsg, errmsg);
		else fprintf (stderr, "%s", errmsg);	    
		free_data_hdr (hdr);
		return (QLIB2_MALLOC_ERROR);
	    }
	    nblks_malloced += MALLOC_INCREMENT;
	}

	/* Initialize the next fixed data header.			*/
	p_ms = *pp_ms + (num_blocks * blksize);
	if (init_miniseed_hdr ((SDR_HDR *)p_ms, hdr, init_bs) < 0) {
	    sprintf (errmsg, "Error: initializing MiniSEED header");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else fprintf (stderr, "%s", errmsg);	    
	    free_data_hdr (hdr);
	    if (nblks_malloced > 0) free(*pp_ms);
	    return (MS_ERROR);
	}
	    
	init_bs = NULL;
	p_packed = (void *)(p_ms + hdr->first_data);
	max_bytes = blksize - hdr->first_data;

	/* Pack the rest of the miniSEED record with data.		*/
	status = pack_fp_sp ((float*)p_packed, &data[ipt], samples_remaining, 
			 max_bytes, pad, hdr->data_wordorder, &nbytes, &nsamples);

	if (status != 0) {
	    sprintf (errmsg, "Error packing %s data\n", "IEEE_FP_SP");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else {
		fprintf (stderr, "%s", errmsg);
		fflush (stderr);
	    }
	}

	/* End of data or Mini-SEED block is full or packing error.	*/
	/* Update Mini-SEED header with:				*/
	/*	final sample count.					*/
	/* Update hdr for the next record.				*/
	hdr->num_samples = nsamples;
	update_miniseed_hdr ((SDR_HDR *)p_ms, hdr);
	ms_pack2_update_hdr (hdr, 1, nsamples, (int*)data+ipt);
	ipt += nsamples;
	samples_remaining -= nsamples;
	++num_blocks;
	hdr->num_samples = 0;
    }

    /* Cleanup.								*/
    free_data_hdr (hdr);
    ms_pack2_update_return_hdr (hdr0, num_blocks, num_samples, (void*)data);
    *n_blocks = num_blocks;
    *n_samples = num_samples - samples_remaining;
    return(status);
}

/************************************************************************/
/*  ms_pack2_double:							*/
/*	Pack data into miniSEED records in IEEE_FP_DP format    	*/
/*									*/
/*	If *pp_ms is NULL, space for the miniSEED records will be	*/
/*	allocated by packing routines, and should be freed by the	*/
/*	calling routine.  Otherwise, the packing routines will use	*/
/*	the space pointed to by *pp_ms.					*/
/*									*/
/*	Return:								*/
/*	    # samples packed on success.				*/
/*	    negative QLIB2 error code on error.				*/
/************************************************************************/
int ms_pack2_double 
   (DATA_HDR	*hdr0,		/* ptr to initial data hdr.		*/
    BS		*init_bs,	/* ptr to onetime blockettes.		*/
    double	*data,		/* ptr to data buffer.			*/
    int		num_samples,	/* number of data samples.		*/
    int		*n_blocks,	/* # miniSEED blocks (returned).	*/
    int		*n_samples,	/* # data samples packed (returned).	*/
    char	**pp_ms,	/* ptr **miniSEED (returned).		*/
    int		ms_len,		/* miniSEED buffer len (if supplied).	*/
    char	*p_errmsg)	/* ptr to error msg buffer.		*/
{
    DATA_HDR *hdr;		/* data header used for writing miniSEED*/
    char *p_ms;			/* ptr to current miniSEED block.	*/
    void *p_packed;		/* ptr to packed output data.		*/
    char errmsg[256];		/* error msg buffer.			*/
    int ipt;			/* index of data to pack.		*/
    int nblks_malloced;		/* # of miniSEED output blocks malloced.*/
    int num_blocks;		/* # of miniSEED block created.		*/
    int samples_remaining;	/* # samples left to cvt to miniseed.	*/
    int nsamples;		/* # of samples in miniSEED block.	*/
    int max_bytes;		/* max # of data bytes in record.	*/
    int nbytes;			/* # of bytes packed into record.	*/
    int pad;			/* flag to indicate padding of frames.	*/
    int blksize = hdr0->blksize;/* output blksize.			*/
    int status = 0;		/* status from data packing routine.	*/

    /* Initialization.							*/
    *n_blocks = 0;

    /* Check for invalid arguments.					*/
    if (num_samples <= 0) return(MS_ERROR);
    if (blksize < 128 ||
	(blksize != (int)pow(2.0,floor(log2((double)blksize))))) {
	sprintf (errmsg, "Warning: invalid blksize: %d\n", blksize);
	if (p_errmsg) strcpy(p_errmsg, errmsg);
	else fprintf (stderr, "%s", errmsg);
	return (MS_ERROR);
    }

    /* If *pp_ms != NULL, assume that the caller is providing sufficient*/
    /* memory to hold all of the resulting miniSEED records.		*/
    /* If it is NULL, we allocate the space, and set it to point to the	*/
    /* allocated miniSEED records.					*/
    /* If we allocated the space for the miniSEED, the caller is	*/
    /* responsible for freeing the space.				*/
    if (*pp_ms) nblks_malloced = -1;
    else nblks_malloced = 0;

    /* Create a copy of the initial data_hdr for our use.		*/
    /* We will update this each time we create a miniSEED block.	*/
    hdr = dup_data_hdr (hdr0);
    if (hdr == NULL) {
	return (MS_ERROR);
    }

    /* Start compressor.						*/
    num_blocks = 0;
    samples_remaining = num_samples;
    ipt = 0;
    pad = 1;

    while (samples_remaining > 0 && status == 0) {
	/* Check for available space.					*/
	/* Allocate more space for Mini-SEED blocks if necessary.	*/
	if (nblks_malloced < 0) {
	    if (ms_len < blksize) {
		*n_blocks = num_blocks;
		free_data_hdr (hdr);
		return (num_samples - samples_remaining);
	    }
	    ms_len -= blksize;
	}
	if (nblks_malloced >= 0 && num_blocks == nblks_malloced) {
	    if(*pp_ms == NULL) {
	      *pp_ms = (char *)malloc((nblks_malloced+MALLOC_INCREMENT)*blksize);
	    } else {
	      *pp_ms = (char *)realloc(*pp_ms,(nblks_malloced+MALLOC_INCREMENT)*blksize);
	    }
	    if (*pp_ms == NULL) {
		sprintf (errmsg, "Error mallocing miniSEED buffer\n");
		if (p_errmsg) strcpy(p_errmsg, errmsg);
		else fprintf (stderr, "%s", errmsg);	    
		free_data_hdr (hdr);
		return (QLIB2_MALLOC_ERROR);
	    }
	    nblks_malloced += MALLOC_INCREMENT;
	}

	/* Initialize the next fixed data header.			*/
	p_ms = *pp_ms + (num_blocks * blksize);
	if (init_miniseed_hdr ((SDR_HDR *)p_ms, hdr, init_bs) < 0) {
	    sprintf (errmsg, "Error: initializing MiniSEED header");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else fprintf (stderr, "%s", errmsg);	    
	    free_data_hdr (hdr);
	    if (nblks_malloced > 0) free(*pp_ms);
	    return (MS_ERROR);
	}
	    
	init_bs = NULL;
	p_packed = (void *)(p_ms + hdr->first_data);
	max_bytes = blksize - hdr->first_data;

	/* Pack the rest of the miniSEED record with data.		*/
	status = pack_fp_dp ((double*)p_packed, &data[ipt], samples_remaining, 
			 max_bytes, pad, hdr->data_wordorder, &nbytes, &nsamples);

	if (status != 0) {
	    sprintf (errmsg, "Error packing %s data\n", "IEEE_FP_DP");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else {
		fprintf (stderr, "%s", errmsg);
		fflush (stderr);
	    }
	}

	/* End of data or Mini-SEED block is full or packing error.	*/
	/* Update Mini-SEED header with:				*/
	/*	final sample count.					*/
	/* Update hdr for the next record.				*/
	hdr->num_samples = nsamples;
	update_miniseed_hdr ((SDR_HDR *)p_ms, hdr);
	ms_pack2_update_hdr (hdr, 1, nsamples, (int*)data+ipt);
	ipt += nsamples;
	samples_remaining -= nsamples;
	++num_blocks;
	hdr->num_samples = 0;
    }

    /* Cleanup.								*/
    free_data_hdr (hdr);
    ms_pack2_update_return_hdr (hdr0, num_blocks, num_samples, (void *)data);
    *n_blocks = num_blocks;
    *n_samples = num_samples - samples_remaining;
    return(status);
}

/************************************************************************/
/*  ms_pack2_text:							*/
/*	Pack text into miniSEED records in unknown format.		*/
/*									*/
/*	If *pp_ms is NULL, space for the miniSEED records will be	*/
/*	allocated by packing routines, and should be freed by the	*/
/*	calling routine.  Otherwise, the packing routines will use	*/
/*	the space pointed to by *pp_ms.					*/
/*									*/
/*	Return:								*/
/*	    # samples packed on success.				*/
/*	    negative QLIB2 error code on error.				*/
/************************************************************************/
int ms_pack2_text
   (DATA_HDR	*hdr0,		/* ptr to initial data hdr.		*/
    BS		*init_bs,	/* ptr to onetime blockettes.		*/
    char	*data,		/* ptr to data buffer.			*/
    int		num_samples,	/* number of data samples.		*/
    int		*n_blocks,	/* # miniSEED blocks (returned).	*/
    int		*n_samples,	/* # data samples packed (returned).	*/
    char	**pp_ms,	/* ptr **miniSEED (returned).		*/
    int		ms_len,		/* miniSEED buffer len (if supplied).	*/
    char	*p_errmsg)	/* ptr to error msg buffer.		*/
{
    DATA_HDR *hdr;		/* data header used for writing miniSEED*/
    char *p_ms;			/* ptr to current miniSEED block.	*/
    void *p_packed;		/* ptr to packed output data.		*/
    char errmsg[256];		/* error msg buffer.			*/
    int ipt;			/* index of data to pack.		*/
    int nblks_malloced;		/* # of miniSEED output blocks malloced.*/
    int num_blocks;		/* # of miniSEED block created.		*/
    int samples_remaining;	/* # samples left to cvt to miniseed.	*/
    int nsamples;		/* # of samples in miniSEED block.	*/
    int max_bytes;		/* max # of data bytes in record.	*/
    int nbytes;			/* # of bytes packed into record.	*/
    int pad;			/* flag to indicate padding of frames.	*/
    int blksize = hdr0->blksize;/* output blksize.			*/
    int status = 0;		/* status from data packing routine.	*/

    /* Initialization.							*/
    *n_blocks = 0;

    /* Check for invalid arguments.					*/
    if (num_samples <= 0) return(MS_ERROR);
    if (blksize < 128 ||
	(blksize != (int)pow(2.0,floor(log2((double)blksize))))) {
	sprintf (errmsg, "Warning: invalid blksize: %d\n", blksize);
	if (p_errmsg) strcpy(p_errmsg, errmsg);
	else fprintf (stderr, "%s", errmsg);
	return (MS_ERROR);
    }

    /* If *pp_ms != NULL, assume that the caller is providing sufficient*/
    /* memory to hold all of the resulting miniSEED records.		*/
    /* If it is NULL, we allocate the space, and set it to point to the	*/
    /* allocated miniSEED records.					*/
    /* If we allocated the space for the miniSEED, the caller is	*/
    /* responsible for freeing the space.				*/
    if (*pp_ms) nblks_malloced = -1;
    else nblks_malloced = 0;

    /* Create a copy of the initial data_hdr for our use.		*/
    /* We will update this each time we create a miniSEED block.	*/
    hdr = dup_data_hdr (hdr0);
    if (hdr == NULL) {
	return (MS_ERROR);
    }

    /* Start compressor.						*/
    num_blocks = 0;
    samples_remaining = num_samples;
    ipt = 0;
    pad = 1;

    while (samples_remaining > 0 && status == 0) {
	/* Check for available space.					*/
	/* Allocate more space for Mini-SEED blocks if necessary.	*/
	if (nblks_malloced < 0) {
	    if (ms_len < blksize) {
		*n_blocks = num_blocks;
		free_data_hdr (hdr);
		return (num_samples - samples_remaining);
	    }
	    ms_len -= blksize;
	}
	if (nblks_malloced >= 0 && num_blocks == nblks_malloced) {
	    *pp_ms = (*pp_ms == NULL) ?
		(char *)malloc((nblks_malloced+MALLOC_INCREMENT)*blksize) :
		(char *)realloc(*pp_ms,(nblks_malloced+MALLOC_INCREMENT)*blksize);
	    if (*pp_ms == NULL) {
		sprintf (errmsg, "Error mallocing miniSEED buffer\n");
		if (p_errmsg) strcpy(p_errmsg, errmsg);
		else fprintf (stderr, "%s", errmsg);	    
		free_data_hdr (hdr);
		return (QLIB2_MALLOC_ERROR);
	    }
	    nblks_malloced += MALLOC_INCREMENT;
	}

	/* Initialize the next fixed data header.			*/
	p_ms = *pp_ms + (num_blocks * blksize);
	if (init_miniseed_hdr ((SDR_HDR *)p_ms, hdr, init_bs) < 0) {
	    sprintf (errmsg, "Error: initializing MiniSEED header");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else fprintf (stderr, "%s", errmsg);	    
	    free_data_hdr (hdr);
	    if (nblks_malloced > 0) free(*pp_ms);
	    return (MS_ERROR);
	}
	init_bs = NULL;
	p_packed = (void *)(p_ms + hdr->first_data);
	max_bytes = blksize - hdr->first_data;

	/* Pack the rest of the miniSEED record with text lines.	*/
	status = pack_text ((char *)p_packed, (char *)&data[ipt], samples_remaining, 
		   max_bytes, pad, hdr->data_wordorder, &nbytes, &nsamples);

	if (status != 0) {
	    sprintf (errmsg, "Error packing %s data\n", "TEXT");
	    if (p_errmsg) strcpy(p_errmsg, errmsg);
	    else {
		fprintf (stderr, "%s", errmsg);
		fflush (stderr);
	    }
	}

	/* End of data or Mini-SEED block is full or packing error.	*/
	/* Update Mini-SEED header with:				*/
	/*	final sample count.					*/
	/* Update hdr for the next record.				*/
	hdr->num_samples = nsamples;
	update_miniseed_hdr ((SDR_HDR *)p_ms, hdr);
	ms_pack2_update_hdr (hdr, 1, nsamples, (void *)&data[ipt]);
	ipt += nsamples;
	samples_remaining -= nsamples;
	++num_blocks;
	hdr->num_samples = 0;
    }

    /* Cleanup.								*/
    free_data_hdr (hdr);
    ms_pack2_update_return_hdr (hdr0, num_blocks, num_samples, (void *)data);
    *n_blocks = num_blocks;
    *n_samples = num_samples - samples_remaining;
    return(status);
}

/************************************************************************/
/* Fortran interludes to ms_pack routines.				*/
/************************************************************************/

/************************************************************************/
/*  ms_pack2_data_:							*/
/*	Pack data into miniSEED records in specified data format.	*/
/*	FORTRAN version.						*/
/*									*/
/*	Return:								*/
/*	    # samples packed on success.				*/
/*	    negative QLIB2 error code on error.				*/
/************************************************************************/
#ifdef	fortran_suffix
int f_ms_pack2_data_
#else
int f_ms_pack2_data
#endif
   (DATA_HDR	*hdr,		/* ptr to initial data hdr.		*/
    int		*num_samples,	/* number of data samples.		*/
    int		*data,		/* ptr to data buffer.			*/
    int		*n_blocks,	/* # miniSEED blocks (returned).	*/
    int		*n_samples,	/* # data samples packed (returned).	*/
    char	*p_ms,		/* ptr *miniSEED (required).		*/
    int		*ms_len)	/* miniSEED buffer len (required).	*/
{
    int status;
    status = ms_pack2_data (hdr, NULL, *num_samples, data, n_blocks, n_samples,
			   &p_ms, *ms_len, NULL);
    return (status);
}
