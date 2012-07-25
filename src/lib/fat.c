#include "../telemetry.h"


unsigned char fat_init(fat_partition* fat, unsigned char* buf) {

    unsigned int i;
	const unsigned char temp_name[] = "MSDOS";

    // ustaw podany bufor jako bufor operacji systemu plik�w 
    fat_buffer = buf;
    fat_struct = fat;

    // odczytaj pierwszy sektor (przynajmniej spr�buj)
	if( !FAT_READ_SECTOR(0, fat_buffer) ) {
		return 0;
    }

    // zeruj struktur� informacji o partycji
	memset((void*) fat, 0, sizeof(fat_partition));
	
	// sprawdzanie, czy zerowy sektor karty to MASTER Boot Record, czy "Zwykly" Boot Record
	// sprawdzanie poprzez nazwe "MSDOS", ktora w przypadku "Zwyklego" Boot Record'a jest wpsiana na poczatku sektora
	for( i=3; i<8; i++) {
		if(fat_buffer[i]!=temp_name[i-3]) {
			// zerowy sektor to MBR
			fat->part_first_sector_off = fat_buffer[FAT_PART_FIRST_SECTOR];
			fat->type = fat_buffer[FAT_PART_TYPE];
            
            // wczytaj Boot Record partycji
			if( ! FAT_READ_SECTOR(fat->part_first_sector_off, fat_buffer) ) {
			    return 0;	
            }
		}
	}

    // typ partycji jeszcze nie znany?
	if( !fat->type ) {
		unsigned char temp_name1[] = "FAT16";

        //sprawdzanie jaki typ partycji
		for(i=0x36; i<0x36+5; i++) {
			if ( fat_buffer[i] == temp_name1[i-0x36] ) {
			    fat->type = FAT_FAT16;
            }
			else {
			    fat->type = FAT_UNKNOWN;
            }
		}
	}

    // skopiuj opis typu partycji do pola name
    memcpy((void*)&(fat->name), (void*)(fat_buffer+0x36), 5);
    fat->name[5] = 0;

    // ustaw pozosta�e pola struktury opisu partycji
	fat->bytes_per_sector 		= fat_buffer[0x0B] | fat_buffer[0x0C]<<8;	//powinno byc 512
	fat->sectors_per_cluster    = fat_buffer[0x0D];	
	fat->reserved_sectors		= fat_buffer[0x0E] | fat_buffer[0x0F]<<8;
	fat->number_of_fat_tables	= fat_buffer[0x10];
	fat->max_root_dir_entries	= fat_buffer[0x11] | fat_buffer[0x12]<<8;
	fat->sectors_per_fat		= fat_buffer[0x16] | fat_buffer[0x17]<<8;
	fat->total_sectors			= fat_buffer[0x13] | fat_buffer[0x14]<<8;
	fat->fat_off				= fat->part_first_sector_off + fat->reserved_sectors;
	
	fat->root_dir_off			= fat->part_first_sector_off + fat->reserved_sectors +
								  fat->number_of_fat_tables*fat->sectors_per_fat ;	
								  
	fat->first_cluster_off       = fat->root_dir_off + (fat->max_root_dir_entries*32)/fat->bytes_per_sector - 2*fat->sectors_per_cluster;

    rs_dump((unsigned char*)fat, sizeof(fat_partition));

	return 1;	
} 


unsigned int fat_cluster_read(unsigned long cluster) {
    
    unsigned long offset = (cluster << 1) % 512;
	unsigned long sector = cluster >> 8; 
	
	FAT_READ_SECTOR( (fat_struct->fat_off + sector), fat_buffer);

    // pobierz dwa bajty z odczytanego sektora karty
	return (fat_buffer[offset] | (unsigned int) fat_buffer[offset+1] << 8);
}

void fat_cluster_write(unsigned long cluster, unsigned int value) {
    
    unsigned long offset = (cluster << 1) % 512;
	unsigned long sector = cluster >> 8; 
	
    FAT_READ_SECTOR( (fat_struct->fat_off + sector), fat_buffer);

	fat_buffer[offset]   = (unsigned char) value;
	fat_buffer[offset+1] = (unsigned char) (value >> 8);
	
    FAT_WRITE_SECTOR( (fat_struct->fat_off + sector), fat_buffer);
}



unsigned char fat_file_open(fat_file* file, unsigned char* name, unsigned char* ext) {

    unsigned char i;
    unsigned int j;
    unsigned long sector;

    // przygotuj struktur� informacyjn� pliku
    memset((void*) file, 0, sizeof(fat_file));
    
    // wype�nij pole nazwy pliku ...
    for(i=0; i<8; i++) {
	    file->name[i] = fat_validate_char(name[i]);
    }

    // ... i rozszerzenia
    for(i=0; i<3; i++) {
        file->ext[i] = fat_validate_char(ext[i]);
    }

    rs_dump((void*)file, sizeof(fat_file));
    
    // szukaj pliku w katalogu nadrz�dnym -> mo�e plik istnieje
	for(sector = fat_struct->root_dir_off; sector < (fat_struct->root_dir_off + fat_struct->max_root_dir_entries/16); sector++)
	{			
		FAT_READ_SECTOR(sector, fat_buffer);
			
        // szukaj w�r�d kolejnych plik�w
		for(j=0; j<512; j+=32)
		{
            // znaleziono plik?
			if( fat_check_root_dir_entry(file, (unsigned char*) (fat_buffer+j)) ) {

				file->first_cluster             = fat_buffer[j+26] | (unsigned int)fat_buffer[j+27]<<8;
				file->size			            = (unsigned long)(fat_buffer[j+28]) | (unsigned long)(fat_buffer[j+29])<<8 | (unsigned long)(fat_buffer[j+30])<<16 | (unsigned long)(fat_buffer[j+31])<<24;	
				file->number_of_entry_in_dir    = (sector - fat_struct->root_dir_off + j) / 32;
				
				// przeszukaj tablic� FAT w poszukiwaniu ostatniego klastra zajmowanego przez plik
				unsigned int cluster_value = fat_cluster_read(file->first_cluster);
				unsigned long cluster = file->first_cluster;

				if (cluster_value < 0xFFF0) {
                    // plik zajmuje wi�cej ni� jeden klaster
					do {
						cluster_value = cluster;
						cluster = fat_cluster_read(cluster_value);			

					} while (cluster < 0xFFF0);
				    
                    file->last_cluster = cluster_value;
				}
				else {
                    // plik w jednym klastrze
                    file->last_cluster = file->first_cluster;
			    }

                rs_dump((void*)file, sizeof(fat_file));
				
                return 1;
			}
        }
    }

    //
    // utw�rz nowy plik
    //

    // przeszukuj katalog nadrz�dny w poszukiwaniu wolnego miejsca na nowy wpis
    for(sector = fat_struct->root_dir_off; sector < (fat_struct->root_dir_off + fat_struct->max_root_dir_entries/16); sector++)
	{
		FAT_READ_SECTOR(sector, fat_buffer);
	
		for(j=0; j<512; j+=32) {
			if(fat_buffer[j] == 0 || fat_buffer[j] == 0xE5)
			{
                // wolny wpis
				file->number_of_entry_in_dir = ((sector-fat_struct->root_dir_off)*16 + j/32);
				file->size = 1;
				break;
			}			
		}

		if(file->size)
			break;
    }

    // nie uda�o znale�� si� wolnego miejsca na nowy wpis
    if (!file->size) {
        return 0;
    }

    // zeruj rozmiar pliku -> pole u�yte jako flaga przy szukaniu wolnego miejsca na wpis
    file->size = 0;

    rs_dump((void*)file, sizeof(fat_file));

    sector = fat_find_free_cluster();
			
    // nie ma ju� wolnego miejsca
	if (sector >= 0xFFF0) {
	    return 0;
    }

    // wst�pne ustawienie informacji o pliku (zajmowane sektory)
    file->first_cluster = file->last_cluster  = sector;
		
    // nowy plik zajmuje jeden klaster
    fat_cluster_write(file->first_cluster, 0xFFFF);	
	
    // odczytaj dane z katalogu nadrz�dnego
    sector = fat_struct->root_dir_off + (file->number_of_entry_in_dir >> 4); // /16
    FAT_READ_SECTOR(sector, fat_buffer);

    i = 0;

    unsigned long offset = (file->number_of_entry_in_dir << 5) % 512; // 2^5 = 32
	
    //
    // dokonaj wpisu do katalogu nadrz�dnego
    //

    // nazwa
    for(i=0; i<8; i++) {
        fat_buffer[offset+i] = file->name[i];
    }

    // rozszerzenie
    for (i=0; i<3; i++) {
	    fat_buffer[offset+i+8] = file->ext[i];
    }
	
    // atrybuty pliku: archiwalny
	fat_buffer[offset+0x0b] = 0x20;

    // numer pierwszego klastra pliku
	fat_buffer[offset+0x1a] = file->first_cluster & 0x00FF;
	fat_buffer[offset+0x1b] = file->first_cluster >> 8;
				
    // rozmiar pliku
	fat_buffer[offset+0x1c] = file->size & 0x0000FF;
	fat_buffer[offset+0x1d] = file->size >> 8;
	fat_buffer[offset+0x1e] = file->size >> 16;

    // zapisz wpis
    FAT_WRITE_SECTOR(sector, fat_buffer);

    rs_dump((void*)file, sizeof(fat_file));

    return 1;
}


unsigned char fat_file_write(fat_file* file, unsigned char* buf, unsigned int len) {
    
    // czy mamy otwarty plik?
    if (!file->name[0]) {
        return 0;
    }

    // liczba sektor�w i klastr�w zajmowanych przez plik - 1
    unsigned long file_sectors = file->size / 512;
    unsigned long file_clusters = file_sectors / fat_struct->sectors_per_cluster;

    // do kt�rego sektora odb�dzie si� zapis
    unsigned long sector = fat_struct->first_cluster_off;
    
    //sector += fat_struct->sectors_per_cluster * (unsigned long) tablica_klastrow[clusters_in_row] + file_sector%fat_struct->sectors_per_cluster;

    // zapisz
    FAT_WRITE_SECTOR(sector, buf);
	
	file->cur_pos 		+= len;
	file->size    		+= len;
	//plik->last_cluster 	=  tablica_klastrow[clusters_in_row];

    return 1;
}



// dokonuje zamiany liter na wielkie, znaki nieobs�ugiwane przez FAT zamieniane na 'X'
unsigned char fat_validate_char(unsigned char c)
{
    if( (c<0x20) || (c>0x20&&c<0x30&&c!='-') || (c>0x39&&c<0x41) || (c>0x5A&&c<0x61&&c!='_') ||	(c>0x7A&&c!='~') )
		return 0x58;    // X
    if( c>=0x61 && c<=0x7A )
		return c-32;    // wielka litera

	return c;           // ok
}

// por�wnuje podany wpis w katalogu nadrz�dnym 
unsigned char fat_check_root_dir_entry(fat_file* file, unsigned char* buf) {

    unsigned char i;

    // porownaj nazwe pliku
	for(i=0; i<8; i++) {
		if( file->name[i] != buf[i] ) {
            return 0;
        }
	}

    // i rozszerzenie
	for(i=0; i<3; i++) {
		if( file->ext[i] != buf[8+i] ) {
            return 0;
        }
	}

    return 1;
}

// znajduje numer pierwszego wolnego klustra
unsigned long fat_find_free_cluster(void) {

	unsigned int sector;
	unsigned int i;
	unsigned int val;
	unsigned int cluster_counter=0;		
	
	for(sector = 0; sector < fat_struct->sectors_per_fat; sector++) {		
		
        FAT_READ_SECTOR(fat_struct->fat_off + sector, fat_buffer);
		
		for(i=0; i<512; i+=2) {
			val = (unsigned int)fat_buffer[i] | (unsigned int)fat_buffer[i+1]<<8;			

			if(val == 0) {
				return cluster_counter;
            }

			cluster_counter++;
		}
	}

	return 0xFFFF;
}
