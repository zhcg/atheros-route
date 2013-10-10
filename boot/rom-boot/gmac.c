#include <asm/addrspace.h>
#include <wasp_api.h>
#include <gmac_api.h>
#include <gmac_fwd.h>
#include <ar7240_soc.h>
#include <phy.h>
#include <apb_map.h>

#if 0
#	define gmacdbg	A_PRINTF
#else
#	define gmacdbg(...)
#endif
#define g_print	A_PRINTF

athr_gmac_desc_t	tx_desc[ATHR_NUM_TX_DESC],
			rx_desc[ATHR_NUM_RX_DESC];

uint8_t			tx_buf[ATHR_NUM_TX_DESC][ATHR_TX_BUF_SIZE],
			rx_buf[ATHR_NUM_RX_DESC][ATHR_RX_BUF_SIZE];

uint32_t		ge_base[] = { AR7240_GE0_BASE, AR7240_GE1_BASE };

__gmac_hdr_t gmac_hdr = {
	.eth = {
		.dst	= { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.src	= { 0x00, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa },
		.etype	= ETH_P_ATH
	},
	.ath = {
		.proto	= ATH_P_BOOT
	}
};

inline int
is_ath_header(__gmac_hdr_t *hdr)
{
	return (hdr->ath.proto == gmac_hdr.ath.proto);
}

void
gmac_mac_setup(void)
{
	//athr_gmac_reg_rmw_set(ATHR_BOOTROM_MAC, ATHR_GMAC_CFG2, ATHR_GMAC_CFG2_VAL);
	athr_gmac_reg_rmw_set(ATHR_BOOTROM_MAC, ATHR_GMAC_CFG2, 0x7135);
	gmacdbg("%s: gmac cfg2 0x%x\n", __func__, athr_gmac_reg_rd(ATHR_BOOTROM_MAC, ATHR_GMAC_CFG2));

	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_FIFO_CFG_0, 0x1f00);
	gmacdbg("%s: gmac fifo_cfg0 0x%x\n", __func__, athr_gmac_reg_rd(ATHR_BOOTROM_MAC, ATHR_GMAC_FIFO_CFG_0));

	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_CFG1,
			(ATHR_GMAC_CFG1_RX_EN | ATHR_GMAC_CFG1_TX_EN));
	gmacdbg("%s: gmac cfg1 0x%x\n", __func__, athr_gmac_reg_rd(ATHR_BOOTROM_MAC, ATHR_GMAC_CFG1));

	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_GE0_MAC_ADDR1, 0xaaaaaaaa);
	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_GE0_MAC_ADDR2, 0xaaaa0000);


	//athr_gmac_reg_rmw_set(ATHR_BOOTROM_MAC, ATHR_GMAC_CFG1, ATHR_GMAC_CFG1_RX_FCTL);

#if (ATHR_BOOTROM_MAC == 1)
	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_MII_MGMT_CFG, 7 | (1 << 31));
	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_MII_MGMT_CFG, 7);
#endif

	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_FIFO_CFG_1, 0x10ffff);
	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_FIFO_CFG_2, 0x015500aa);
	/*
	 * Weed out junk frames (CRC errored, short collision'ed frames etc.)
	 */
	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_FIFO_CFG_3, 0x1f00140);

	// athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_FIFO_CFG_4, 0x1000);
	// athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_FIFO_CFG_5, 0x66b82);

	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_DMA_TX_DESC, PHYSADDR(&tx_desc[0]));

	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_DMA_RX_DESC, PHYSADDR(&rx_desc[0]));
	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_DMA_RX_CTRL, ATHR_GMAC_RXE);

	athr_gmac_reg_rd(ATHR_BOOTROM_MAC, ATHR_GMAC_CFG2);
}

void
gmac_phy_setup(void)
{
	uint32_t	rd_data;

	rd_data = athrs27_reg_read(0x4);
	gmacdbg("port0_pad_ctrl_reg  0x%x\n", rd_data);
	athrs27_reg_write(0x4,(rd_data|(1<<6)));

	rd_data = athrs27_reg_read(0x4);
	g_print("port0_pad_ctrl_reg  0x%x\n", rd_data);

	rd_data = athrs27_reg_read(0x2c);
	gmacdbg("0x2c register before setting: 0x%x\n", rd_data);
	athrs27_reg_write(0x2c,(rd_data|(3<<25)| (1<<16) | 0x1));

	rd_data = athrs27_reg_read(0x2c);
	gmacdbg("0x2c register after setting: 0x%x\n", rd_data);

	rd_data = athrs27_reg_read(0x8);
	athrs27_reg_write(0x8,(rd_data|(1<<28)));

	phy_reg_write(1, ATHR_PHY4_ADDR, ATHR_PHY_CONTROL, 0x9000);
	g_print("Wait for Auto-neg to complete...\n");
	while(1) {
		if (phy_reg_read(1, ATHR_PHY4_ADDR, 0x1) & (1 << 5)) {
			break;
		}
		udelay(100);
		gmacdbg(".");
	}
	g_print("done. status = 0x11 = 0x%x\n", phy_reg_read(1, ATHR_PHY4_ADDR, 0x11));

	/* Enable MDIO Access if PHY is Powered-down */
	phy_reg_write(1, ATHR_PHY4_ADDR, ATHR_DEBUG_PORT_ADDRESS, 0x3);
	rd_data = phy_reg_read(1, ATHR_PHY4_ADDR, ATHR_DEBUG_PORT_DATA);
	phy_reg_write(1, ATHR_PHY4_ADDR, ATHR_DEBUG_PORT_ADDRESS, 0x3);
	phy_reg_write(1, ATHR_PHY4_ADDR, ATHR_DEBUG_PORT_DATA, (rd_data & 0xfffffeff));
}


void
gmac_init(void)
{
	unsigned int		val, fifo_3;
	athr_gmac_desc_t	*p;

	fifo_3 = 0x000001ff | (ATHR_GMAC_TX_FIFO_LEN / 4) << 16;

#if 1
	athr_gmac_reg_rmw_set(ATHR_BOOTROM_MAC, ATHR_GMAC_CFG1,
				ATHR_GMAC_CFG1_SOFT_RST |
				ATHR_GMAC_CFG1_RX_RST |
				ATHR_GMAC_CFG1_TX_RST);
#endif

	//ar7240_reg_wr(SWITCH_CLOCK_SPARE_ADDRESS,
		//ar7240_reg_rd(SWITCH_CLOCK_SPARE_ADDRESS) | 0x70);

	// Pull switch analog, GE0 & MDIOs out of reset
	ar7240_reg_rmw_clear(ATHR_RESET, RST_RESET_ETH_SWITCH_ARESET_MASK);
	udelay(250);	// 250 usecs

	ar7240_reg_rmw_clear(ATHR_RESET, RST_RESET_ETH_SWITCH_RESET_MASK);
	udelay(1);	// 1 usecs

	ar7240_reg_rmw_clear(ATHR_RESET,
			RST_RESET_GE0_MAC_RESET_MASK |
			RST_RESET_GE1_MDIO_RESET_MASK);
	udelay(1);

	// This is for using APB to configure the s26 switch
	// instead of mdio.
	// ar7240_reg_rmw_set(AR7240_MII0_CTRL, ETH_CFG_SW_APB_ACCESS_MASK);

	for (val = 0; val < ATHR_NUM_TX_DESC; val++) {
		p = &tx_desc[val];
		p->is_empty		= TX_SW_OWN;
		p->next_desc		= PHYSADDR(&tx_desc[(val + 1) %
							ATHR_NUM_TX_DESC]);
		p->pkt_start_addr	= PHYSADDR(tx_buf[val]);
		p->pkt_size		= ATHR_TX_BUF_SIZE;
	}

	for (val = 0; val < ATHR_NUM_RX_DESC; val++) {
		p = &rx_desc[val];
		p->is_empty		= RX_HW_OWN;
		p->next_desc		= PHYSADDR(&rx_desc[(val + 1) %
							ATHR_NUM_RX_DESC]);
		p->pkt_start_addr	= PHYSADDR(rx_buf[val]);
		p->pkt_size		= ATHR_RX_BUF_SIZE;
	}

	athr_gmac_reg_wr(1, ATHR_GMAC_MII_MGMT_CFG, 0xb);
}

void
gmac_load_fw(void)
{
	int		i;
	while (1) {
		for (i = 0;i < ATHR_NUM_RX_DESC; i++) {
			if (rx_desc[i].is_empty == RX_SW_OWN) {
				GMAC_RX_PKT(&rx_desc[i], rx_buf[i]);
				rx_desc[i].is_empty = RX_HW_OWN;
				athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_DMA_RX_CTRL,
				ATHR_GMAC_RXE);
			}
		}
	}
}

void
gmac_discover(void)
{
	athr_gmac_txbuf_t	tb;
	int			i, ret = 0;

	g_print("gmac_discover: sending discovery...\n");
	while (ret == 0) {

		GMAC_ALLOC_TX_BUF(&tb);

		if (tb.buf == NULL) {
			g_print("%s: Cannot allocate Tx buf\n", __func__);
			return;
		}

		tb.datalen += 0; /* We dont append any data */

		GMAC_TX_PKT(&tb);

		for (i = 0;i < ATHR_NUM_RX_DESC; i++) {
			//gmacdbg("%s: rx_desc[i].is_empty = 0x%x\n",
			//	__func__, rx_desc[i].is_empty);
			if (rx_desc[i].is_empty == RX_SW_OWN) {
				ret = GMAC_RX_DISC(&rx_desc[i], rx_buf[i]);
				rx_desc[i].is_empty = RX_HW_OWN;
				break;
			}
		}
	}
}

void
gmac_fwd_main()
{
	GMAC_DISCOVER();

	while (1) {
		GMAC_LOAD_FW();
	}
}

int
gmac_rx_disc(athr_gmac_desc_t *d, uint8_t *buf)
{
	__gmac_hdr_t	*hdr = gmac_hdr(buf);

	if (!is_ath_header(hdr)) {
		g_print("gmac_rx_disc: ath hdr mismatch\n");
		return 0;
	}

	A_MEMCPY(gmac_hdr.eth.dst, hdr->eth.src, ETH_ALEN);

	g_print("Discover response received\n\t"
		"%02x:%02x:%02x:%02x:%02x:%02x\n",
			gmac_hdr.eth.dst[0], gmac_hdr.eth.dst[1],
			gmac_hdr.eth.dst[2], gmac_hdr.eth.dst[3],
			gmac_hdr.eth.dst[4], gmac_hdr.eth.dst[5]);

	return 1;
}

void
gmac_rx_pkt(athr_gmac_desc_t *d, uint8_t *buf)
{
#define double_check	1
#define dump_pkt	0
	fwd_cmd_t		*c;
	fwd_rsp_t		*r;
	uint32_t		*img,
				*daddr,
#if double_check
				pktsz,
#endif
				len;
#if dump_pkt
	int			i;
#endif
	static uint32_t		fw_size;
	athr_gmac_txbuf_t	tb;
	extern fwd_tgt_softc_t	fwd_sc;

	gmacdbg("--------> %s\n", __func__);
	if (!is_ath_header((void *)buf)) {
		gmacdbg("%s: no ath header\n", __func__);
		return;
	}

	buf += sizeof gmac_hdr;
	gmacdbg("d->pkt_size = %u\n", d->pkt_size);
#if double_check
	pktsz = (d->pkt_size - GMAC_HLEN);
#endif

	c = (fwd_cmd_t *)buf;
	img = (uint32_t *)(buf + sizeof(*c));

	if (c->offset == 0) { /* First Packet */
		fwd_sc.addr = *img;
		fw_size = 0;
		img ++;
		g_print("%s: fwd_sc.addr = 0x%x\n", __func__, fwd_sc.addr);
	}

	daddr = (uint32_t *)(fwd_sc.addr + c->offset);

#if double_check
	if (c->len > pktsz) {
		gmacdbg("%s: Invalid packet len: %u %u\n",
				__func__, c->len, d->pkt_size);
		return;
	}
#endif

	gmacdbg("enter %s: 0x%x 0x%x\n", __func__, c->offset, c->len);
	if (c->more_data) {
		len = c->len;
	} else {	/* skip checksum */
		len = c->len - 4;
	}

	gmacdbg("\t0x%x 0x%x\n", daddr, img);
	g_print(".");
	A_MEMCPY(daddr, img, len);

	GMAC_ALLOC_TX_BUF(&tb);

	r = (fwd_rsp_t *)tb.data;

	fw_size += len;
	r->offset = c->offset;

	if (c->more_data) {
		r->rsp    = FWD_RSP_ACK;
		gmacdbg("%s: send ack\n", __func__);
#if dump_pkt
		img = (uint32_t *)tb.buf;
		for (i = 0; i < 32; i += 4) {
			gmacdbg("\t0x%08x 0x%08x 0x%08x 0x%08x\n",
				img[i + 0], img[i + 1], img[i + 2], img[i + 3]);
		}
#endif

	} else {
		uint32_t	cs;

		cs = checksum(fwd_sc.addr, fw_size);
		img += (len / sizeof(*img));

		if (cs == *img) {
			r->rsp = FWD_RSP_SUCCESS;
			g_print("%s: send success\n", __func__);
		} else {
			g_print("%s: Checksum mismatch 0x%x != 0x%x\n",
					__func__, cs, *img);
			r->rsp = FWD_RSP_FAILED;
			g_print("%s: send failed\n", __func__);
		}
	}

	tb.datalen += sizeof(*r);
	GMAC_TX_PKT(&tb);

	if (r->rsp == FWD_RSP_SUCCESS && !c->more_data) {
		img ++;
		g_print("Download complete. Jumping to %p\n", *img);
		call_fw(HIF_S27, *img);
		g_print("* * * * return from 1st fw * * * *\n", *img);
	}
}

void
gmac_alloc_tx_buf(athr_gmac_txbuf_t *tb)
{
	static uint32_t		tx_ix;

	tb->buf = tx_buf[tx_ix];
	tb->desc = &tx_desc[tx_ix];
	tb->buflen = ATHR_TX_BUF_SIZE;

	gmac_hdr.eth.etype = ETH_P_ATH;
	A_MEMCPY(tb->buf, &gmac_hdr, sizeof gmac_hdr);

	tb->data = tb->buf + sizeof gmac_hdr;
	tb->datalen = sizeof gmac_hdr;

	tx_ix = (tx_ix + 1) % ATHR_NUM_TX_DESC;
}

void
gmac_tx_pkt(athr_gmac_txbuf_t *tb)
{
	tb->desc->is_empty = TX_HW_OWN;
	tb->desc->pkt_size = tb->datalen;

	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_DMA_TX_DESC,
				PHYSADDR(tb->desc));
	athr_gmac_reg_wr(ATHR_BOOTROM_MAC, ATHR_GMAC_DMA_TX_CTRL,
				ATHR_GMAC_TXE);

	while (tb->desc->is_empty == TX_HW_OWN);
}



void
gmac_module_install(struct gmac_api *api)
{
	api->_gmac_init		= gmac_init;
	api->_gmac_mac_setup	= gmac_mac_setup;
	api->_gmac_phy_setup	= gmac_phy_setup;
	api->_gmac_load_fw	= gmac_load_fw;
	api->_gmac_rx_pkt	= gmac_rx_pkt;
	api->_gmac_rx_disc	= gmac_rx_disc;
	api->_gmac_tx_pkt	= gmac_tx_pkt;
	api->_gmac_discover	= gmac_discover;
	api->_gmac_alloc_tx_buf	= gmac_alloc_tx_buf;
	api->_gmac_fwd_main	= gmac_fwd_main;
}
